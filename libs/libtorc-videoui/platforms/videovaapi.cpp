/* Class VideoVAAPI
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012-13
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Qt
#include <QGLWidget>
#include <QMutex>
#include <QLibrary>

// Torc
#include "torclocalcontext.h"
#include "torclogging.h"
#include "videodecoder.h"
#include "videobuffers.h"
#include "videocolourspace.h"
#include "opengl/uiopenglwindow.h"
#include "opengl/uiopengltextures.h"
#include "videoplayer.h"
#include "videovaapi.h"

#include <GL/glx.h>

/*! \class VideoVAAPI
 *  \brief Support for video decode acceleration through VA-API
 *
 * A note on thread safety, OpenGL/GLX and object lifespan:-
 *
 * Without explicitly running the main Qt event loop, we cannot guarantee
 * thread safe OpenGL calls outside of the main UI thread.
 *
 * But VA-API is a decoder library run from the video decoder thread.
 * So, when agreeing a decoder/pixel format with libav, we register a callback
 * with the main UIOpenGLWindow object, wait for the callback and create the VA-API
 * instance within the main thread while the main OpenGL context is current.
 *
 * A similar mechanism is used to destroy the VA-API instance from the main thread.
 *
 * But decoder destruction in the video decoder thread may not correspond with videobuffer
 * destruction/resetting in the UI thread and when switching decoders (streams) or ending playback
 * we want the already decoded frames (owned by VA-API) to play out seemlessly even AFTER the
 * video stream has been closed in the decoder thread.
 *
 * So, VideoVAAPI is reference counted with each VideoFrame holding an additional reference.
 * When the videobuffers are reset or when the VideoVAAPI object is marked as 'Deleting',
 * the VideoFrame releases the VideoVAAPI object and deletes its reference to it. The callback
 * to the main UI thread and final destruction of the VideoVAAPI object only occurs when the
 * reference count has dropped to one.
 *
 * Clear?
 *
 * \todo Fix frame ordering with H.264 content
*/

static QMutex*  gVAAPILock = new QMutex(QMutex::Recursive);

#define VA_ERROR(Type, Message) LOG(VB_GENERAL, LOG_ERR, QString("%1 (%2)").arg(Message).arg(vaErrorStr(Type)));

typedef VADisplay (*VAAPIGetDisplayX11)     (Display*);
typedef VADisplay (*VAAPIGetDisplayGLX)     (Display*);
typedef VAStatus  (*VAAPICreateSurfaceGLX)  (VADisplay, GLenum, GLuint, void**);
typedef VAStatus  (*VAAPIDestroySurfaceGLX) (VADisplay, void*);
typedef VAStatus  (*VAAPICopySurfaceGLX)    (VADisplay, void*, VASurfaceID, unsigned int);

static class VAAPIX11
{
  public:
    VAAPIX11()
    {
        m_getDisplay = (VAAPIGetDisplayX11)QLibrary::resolve("va-x11", "vaGetDisplay");
        m_valid = m_getDisplay;
    }

    bool m_valid;
    VAAPIGetDisplayX11 m_getDisplay;
} VAAPIX11;

static class VAAPIGLX
{
  public:
    VAAPIGLX()
    {
        m_getDisplayGLX     = (VAAPIGetDisplayGLX)     QLibrary::resolve("va-glx", "vaGetDisplayGLX");
        m_createSurfaceGLX  = (VAAPICreateSurfaceGLX)  QLibrary::resolve("va-glx", "vaCreateSurfaceGLX");
        m_destroySurfaceGLX = (VAAPIDestroySurfaceGLX) QLibrary::resolve("va-glx", "vaDestroySurfaceGLX");
        m_copySurfaceGLX    = (VAAPICopySurfaceGLX)    QLibrary::resolve("va-glx", "vaCopySurfaceGLX");

        m_valid = m_getDisplayGLX && m_createSurfaceGLX && m_destroySurfaceGLX && m_copySurfaceGLX;
    }

    bool                   m_valid;
    VAAPIGetDisplayGLX     m_getDisplayGLX;
    VAAPICreateSurfaceGLX  m_createSurfaceGLX;
    VAAPIDestroySurfaceGLX m_destroySurfaceGLX;
    VAAPICopySurfaceGLX    m_copySurfaceGLX;
} VAAPIGLX;

QString ProfileToString(VAProfile Profile)
{
    if (VAProfileMPEG2Simple         == Profile) return QString("MPEG2Simple");
    if (VAProfileMPEG2Main           == Profile) return QString("MPEG2Main");
    if (VAProfileMPEG4Simple         == Profile) return QString("MPEG4Simple");
    if (VAProfileMPEG4AdvancedSimple == Profile) return QString("MPEG4AdvSimple");
    if (VAProfileMPEG4Main           == Profile) return QString("MPEG4Main");
    if (VAProfileH264Baseline        == Profile) return QString("H264Base");
    if (VAProfileH264Main            == Profile) return QString("H264Main");
    if (VAProfileH264High            == Profile) return QString("H264High");
    if (VAProfileVC1Simple           == Profile) return QString("VC1Simple");
    if (VAProfileVC1Main             == Profile) return QString("VC1Main");
    if (VAProfileVC1Advanced         == Profile) return QString("VC1Advanced");
    if (VAProfileH263Baseline        == Profile) return QString("H263Base");
    return QString("Unknown");
}

QString EntrypointToString(VAEntrypoint Entrypoint)
{
    if (VAEntrypointVLD        == Entrypoint) return QString("VLD ");
    if (VAEntrypointIZZ        == Entrypoint) return QString("IZZ (UNSUPPORTED) ");
    if (VAEntrypointIDCT       == Entrypoint) return QString("IDCT (UNSUPPORTED) ");
    if (VAEntrypointMoComp     == Entrypoint) return QString("MC (UNSUPPORTED) ");
    if (VAEntrypointDeblocking == Entrypoint) return QString("Deblock (UNSUPPORTED) ");
    if (VAEntrypointEncSlice   == Entrypoint) return QString("EncSlice (UNSUPPORTED) ");
    return QString("Unknown");
}

void VAAPIUICallback(void* Object, int Parameter)
{
    VideoVAAPI *videovaapi = static_cast<VideoVAAPI*>(Object);

    if (!videovaapi)
        return;

    switch ((VideoVAAPI::CallbackAction)Parameter)
    {
        case VideoVAAPI::Create:
            if (videovaapi->Initialise() && videovaapi->InitialiseContext())
                return;
            LOG(VB_GENERAL, LOG_ERR, "Failed to initialise VAAPI via callback");
            break;
        case VideoVAAPI::Destroy:
            videovaapi->DownRef();
            break;
        default:
            break;
    }
}

QMap<AVCodecContext*,VideoVAAPI*> VideoVAAPI::gVAAPIInstances;

VideoVAAPI* VideoVAAPI::GetVideoVAAPI(AVCodecContext *Context, bool OpenGL)
{
    if (!Context)
        return NULL;

    // library support
    if (!VAAPIAvailable(OpenGL))
        return NULL;

    QMutexLocker locker(gVAAPILock);

    if (gVAAPIInstances.size() > 2)
        LOG(VB_GENERAL, LOG_WARNING, QString("%1 VAAPI instances active").arg(gVAAPIInstances.size()));

    if (gVAAPIInstances.contains(Context))
        return gVAAPIInstances.value(Context);

    VideoVAAPI* videovaapi = new VideoVAAPI(Context, OpenGL);
    if (!videovaapi)
        return NULL;

    TorcReferenceLocker rlocker(videovaapi);

    if (OpenGL)
    {
        // GLX based contexts must be created from the UI/OpenGL thread
        UIOpenGLWindow *window = static_cast<UIOpenGLWindow*>(gLocalContext->GetUIObject());
        if (window)
        {
            window->RegisterCallback(VAAPIUICallback, videovaapi, Create);
            int tries = 0;
            while (!videovaapi->IsReady() && tries++ < 100)
            {
                if (videovaapi->IsErrored() || videovaapi->IsDeleting())
                    break;
                usleep(50000);
            }
        }
    }
    else
    {
        videovaapi->Initialise();
        videovaapi->InitialiseContext();
    }

    if (videovaapi && videovaapi->IsReady())
    {
        gVAAPIInstances.insert(Context, videovaapi);
        return videovaapi;
    }

    return NULL;
}

bool VideoVAAPI::VAAPIAvailable(bool OpenGL)
{
    QMutexLocker locker(gVAAPILock);

    static bool x11available = false;
    static bool glavailable  = false;
    static bool x11checked   = false;
    static bool glchecked    = false;

    if (OpenGL && glchecked)
        return glavailable;

    if (!OpenGL && x11checked)
        return x11available;

    if (OpenGL)
    {
        glchecked = true;
        if (!VAAPIGLX.m_valid)
            return false;
    }
    else
    {
        x11checked = true;
        if (!VAAPIX11.m_valid)
            return false;
    }

    Display *display = XOpenDisplay(NULL);
    if (display)
    {
        VADisplay vadisplay = OpenGL ? VAAPIGLX.m_getDisplayGLX(display) : VAAPIX11.m_getDisplay(display);
        if (vaDisplayIsValid(vadisplay))
        {
            int major;
            int minor;
            if (vaInitialize(vadisplay, &major, &minor) == VA_STATUS_SUCCESS)
            {
                static bool debugged = false;
                if (!debugged)
                {
                    const char* vendor = vaQueryVendorString(display);
                    LOG(VB_GENERAL, LOG_INFO, QString("VAAPI version %1.%2 '%3'").arg(major).arg(minor).arg(vendor));
                    debugged = true;
                }

                if (OpenGL)
                    glavailable = true;
                else
                    x11available = true;

                vaTerminate(vadisplay);
            }
        }

        XCloseDisplay(display);
    }

    return OpenGL ? glavailable : x11available;
}

bool VideoVAAPI::InitialiseDecoder(AVCodecContext *Context, AVPixelFormat Format)
{
    if (!Context || Format != AV_PIX_FMT_VAAPI_VLD)
        return false;

    VideoVAAPI *test = new VideoVAAPI(Context, false, AV_PIX_FMT_VAAPI_VLD);
    if (test)
    {
        bool success = test->Initialise();
        test->Dereference();
        return success;
    }

    return false;
}

void VideoVAAPI::DeinitialiseDecoder(AVCodecContext *Context)
{
    if (!Context)
        return;

    QMutexLocker locker(gVAAPILock);

    if (gVAAPIInstances.contains(Context))
    {
        VideoVAAPI *videovaapi = gVAAPIInstances.take(Context);
        videovaapi->SetDeleting();
        videovaapi->Dereference();
        Context->hwaccel_context = NULL;
        return;
    }

    LOG(VB_GENERAL, LOG_ERR, "Unknown VideoVAAPI instance");
}

VideoVAAPI::VideoVAAPI(AVCodecContext *Context, bool OpenGL, AVPixelFormat TestFormat)
  : TorcReferenceCounter(),
    m_state(Created),
    m_avContext(Context),
    m_lock(new QMutex()),
    m_vendor(Unknown),
    m_opengl(OpenGL),
    m_codec(AV_CODEC_ID_NONE),
    m_level(0),
    m_pixelFormat(AV_PIX_FMT_NONE),
    m_profile(VAProfileMPEG2Simple),
    m_xDisplay(NULL),
    m_vaDisplay(NULL),
    m_numSurfaces(0),
    m_surfaces(NULL),
    m_surfaceData(NULL),
    m_vaapiContext(NULL),
    m_glxSurface(NULL),
    m_surfaceTexture(NULL)
{
    if (m_avContext)
    {
        m_codec = m_avContext->codec_id;
        m_level = m_avContext->level;
        m_pixelFormat = TestFormat != AV_PIX_FMT_NONE ? TestFormat : m_avContext->pix_fmt;
    }
}

VideoVAAPI::~VideoVAAPI()
{
    m_state = Deleted;

    {
        QMutexLocker locker(m_lock);

        if (m_vaDisplay)
        {
            VAStatus status;

            if (m_opengl && m_glxSurface)
            {
                status = VAAPIGLX.m_destroySurfaceGLX(m_vaDisplay, m_glxSurface);
                if (status != VA_STATUS_SUCCESS)
                    VA_ERROR(status, "Failed to destroy GLX surface");
            }

            if (m_vaapiContext && m_vaapiContext->context_id)
            {
                status = vaDestroyContext(m_vaDisplay, m_vaapiContext->context_id);
                if (status != VA_STATUS_SUCCESS)
                    VA_ERROR(status, "Failed to destroy VAAPI context");
            }

            if (m_vaapiContext && m_vaapiContext->config_id)
            {
                status = vaDestroyConfig(m_vaDisplay, m_vaapiContext->config_id);
                if (status != VA_STATUS_SUCCESS)
                    VA_ERROR(status, "Failed to destroy VAAPI config");
            }

            if (m_surfaces)
            {
                status = vaDestroySurfaces(m_vaDisplay, m_surfaces, m_numSurfaces);
                if (status != VA_STATUS_SUCCESS)
                    VA_ERROR(status, "Failed to destroy VAAPI surfaces");
            }
        }

        delete m_vaapiContext;

        if (m_surfaces)
            delete [] m_surfaces;

        if (m_surfaceData)
            delete [] m_surfaceData;

        if (m_vaDisplay)
        {
            vaTerminate(m_vaDisplay);
            m_vaDisplay = NULL;
        }

        if (m_xDisplay && !m_opengl)
        {
            XCloseDisplay(m_xDisplay);
            m_xDisplay = NULL;
        }
    }

    delete m_lock;
    if (m_opengl)
        LOG(VB_GENERAL, LOG_INFO, "Destroyed VideoVAAPI object");
}

bool VideoVAAPI::Dereference(void)
{
    if (m_opengl && !IsShared())
    {
        // destroy from the UI/OpenGL thread. No need to wait for completion
        UIOpenGLWindow *window = static_cast<UIOpenGLWindow*>(gLocalContext->GetUIObject());
        if (window)
            window->RegisterCallback(VAAPIUICallback, this, Destroy);
        return false;
    }

    return TorcReferenceCounter::DownRef();
}

bool VideoVAAPI::IsErrored(void)
{
    return m_state == Errored;
}

bool VideoVAAPI::IsReady(void)
{
    return m_state == Context;
}

bool VideoVAAPI::IsDeleting(void)
{
    return m_state == Deleting;
}

void VideoVAAPI::SetDeleting(void)
{
    m_state = Deleting;
}

bool VideoVAAPI::IsOpenGL(void)
{
    return m_opengl;
}

void VideoVAAPI::Lock(void)
{
    m_lock->lock();
}

void VideoVAAPI::Unlock(void)
{
    m_lock->unlock();
}

bool VideoVAAPI::Initialise(void)
{
    QMutexLocker locker(m_lock);

    if (m_pixelFormat != AV_PIX_FMT_VAAPI_VLD)
    {
        LOG(VB_GENERAL, LOG_ERR, "Invalid VAAPI pixel format");
        m_state = Errored;
        return false;
    }

    // open correct display
    if (m_opengl)
    {
        m_xDisplay = glXGetCurrentDisplay();
    }
    else
    {
        m_xDisplay = XOpenDisplay(NULL);
    }

    if (m_xDisplay)
    {
        m_vaDisplay = m_opengl ? VAAPIGLX.m_getDisplayGLX(m_xDisplay) : VAAPIX11.m_getDisplay(m_xDisplay);

        if (!vaDisplayIsValid(m_vaDisplay))
        {
            LOG(VB_GENERAL, LOG_ERR, "Invalid VADisplay");
            m_state = Errored;
            return false;
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to open X Display");
        m_state = Errored;
        return false;
    }

    // initialise library
    int major;
    int minor;
    VAStatus status = vaInitialize(m_vaDisplay, &major, &minor);

    if (status != VA_STATUS_SUCCESS)
    {
        VA_ERROR(status, "Failed to initialise display");
        m_state = Errored;
        return false;
    }

    // pick a profile
    m_profile = VAProfileMPEG2Simple;
    switch (m_codec)
    {
        case AV_CODEC_ID_H264:
            if (m_level == FF_PROFILE_H264_BASELINE)
            {
                m_profile = VAProfileH264Baseline;
                break;
            }
            else if (m_level == FF_PROFILE_H264_CONSTRAINED_BASELINE)
            {
                m_profile = VAProfileH264ConstrainedBaseline;
                break;
            }
            else if (m_level == FF_PROFILE_H264_MAIN)
            {
                m_profile = VAProfileH264Main;
                break;
            }

            m_profile = VAProfileH264High;
            break;
        case AV_CODEC_ID_MPEG1VIDEO:
        case AV_CODEC_ID_MPEG2VIDEO:
            m_profile = VAProfileMPEG2Main;
            break;
        case AV_CODEC_ID_MPEG4:
        case AV_CODEC_ID_H263:
            m_profile = VAProfileMPEG4AdvancedSimple;
            break;
        case AV_CODEC_ID_WMV3:
            m_profile = VAProfileVC1Main;
            break;
        case AV_CODEC_ID_VC1:
            m_profile = VAProfileVC1Advanced;
            break;
        default:
            break;
    }

    if (m_profile == VAProfileMPEG2Simple)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to find VAAPI profile");
        m_state = Errored;
        return false;
    }

    // check for profile availability and entry point
    int maxprofiles    = vaMaxNumProfiles(m_vaDisplay);
    int maxentrypoints = vaMaxNumEntrypoints(m_vaDisplay);

    if (maxprofiles < 1 || maxentrypoints < 1)
    {
        LOG(VB_GENERAL, LOG_ERR, "No VAAPI profiles available");
        m_state = Errored;
        return false;
    }

    VAProfile *profiles   = new VAProfile[maxprofiles];
    if (!profiles)
    {
        m_state = Errored;
        return false;
    }

    VAEntrypoint *entries = new VAEntrypoint[maxentrypoints];
    if (!entries)
    {
        delete [] profiles;
        m_state = Errored;
        return false;
    }

    int numprofiles = 0;
    bool found = false;
    static bool debugged = false;
    status = vaQueryConfigProfiles(m_vaDisplay, profiles, &numprofiles);
    if (status == VA_STATUS_SUCCESS && numprofiles > 0)
    {
        for (int i = 0; i < numprofiles; ++i)
        {
            if (debugged && (m_profile != profiles[i] || found))
                continue;

            if (!debugged)
                LOG(VB_GENERAL, LOG_INFO, QString("Profile: %1").arg(ProfileToString(profiles[i])));

            int numentries = 0;
            status = vaQueryConfigEntrypoints(m_vaDisplay, profiles[i], entries, &numentries);
            if (status == VA_STATUS_SUCCESS && numentries > 0)
            {
                for (int j = 0; j < numentries; ++j)
                {
                    if (!debugged)
                        LOG(VB_GENERAL, LOG_INFO, QString("\tEntrypoint: %1").arg(EntrypointToString(entries[j])));

                    if (entries[j] == VAEntrypointVLD && profiles[i] == m_profile)
                        found = true;
                }
            }
            else
            {
                VA_ERROR(status, "Failed to retrieve entry points");
            }
        }
    }
    else
    {
        VA_ERROR(status, "Failed to retrieve profiles");
    }

    debugged = true;

    if (!found)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to find entry point for %1").arg(m_profile));
        m_state = Errored;
        return false;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Found entrypoint for %1").arg(ProfileToString(m_profile)));
    m_state = Profile;
    return true;
}

bool VideoVAAPI::InitialiseContext(void)
{
    if (m_state < Profile)
        return false;

    QMutexLocker locker(m_lock);

    // check surface format first;
    VAConfigAttrib attribute;
    attribute.type = VAConfigAttribRTFormat;
    attribute.value = 0;

    VAStatus status = vaGetConfigAttributes(m_vaDisplay, m_profile, VAEntrypointVLD, &attribute, 1);

    if (status != VA_STATUS_SUCCESS || !(attribute.value & VA_RT_FORMAT_YUV420))
    {
        VA_ERROR(status, "Failed to confirm YUV420 chroma");
        m_state = Errored;
        return false;
    }

    // create buffers
    m_numSurfaces = MIN_VIDEO_BUFFERS_FOR_DISPLAY + std::max(MIN_VIDEO_BUFFERS_FOR_DECODE, m_avContext->refs);
    m_surfaces    = new VASurfaceID[m_numSurfaces];
    m_surfaceData = new VAAPISurface[m_numSurfaces];

    if (!m_surfaces || !m_surfaceData)
    {
        m_state = Errored;
        return false;
    }

    memset(m_surfaces,    0, m_numSurfaces * sizeof(VASurfaceID));
    memset(m_surfaceData, 0, m_numSurfaces * sizeof(VAAPISurface));

    status = vaCreateSurfaces(m_vaDisplay, m_avContext->width, m_avContext->height, VA_RT_FORMAT_YUV420, m_numSurfaces, m_surfaces);
    if (status != VA_STATUS_SUCCESS)
    {
        VA_ERROR(status, "Failed to create VAAPI video surfaces");
        m_state = Errored;
        return false;
    }

    // create config
    uint32_t configid;
    status = vaCreateConfig(m_vaDisplay, m_profile, VAEntrypointVLD, &attribute, 1, &configid);
    if (status != VA_STATUS_SUCCESS)
    {
        VA_ERROR(status, "Failed to create VAAPI config");
        m_state = Errored;
        return false;
    }

    // create context
    uint32_t contextid;
    status = vaCreateContext(m_vaDisplay, configid, m_avContext->width, m_avContext->height, VA_PROGRESSIVE,
                             m_surfaces, m_numSurfaces, &contextid);
    if (status != VA_STATUS_SUCCESS)
    {
        VA_ERROR(status, "Failed to create VAAPI context");
        m_state = Errored;
        return false;
    }

    delete m_vaapiContext;
    m_vaapiContext = new vaapi_context();
    memset(m_vaapiContext, 0, sizeof(vaapi_context));
    m_vaapiContext->display    = m_vaDisplay;
    m_vaapiContext->config_id  = configid;
    m_vaapiContext->context_id = contextid;

    for (int i = 0; i < m_numSurfaces; i++)
    {
        m_surfaceData[i].m_id    = m_surfaces[i];
        m_surfaceData[i].m_owner = this;
        UpRef();
    }

    // deinterlacing support on intel
    const char* vendor = vaQueryVendorString(m_vaDisplay);
    QString  vendorstr = QString::fromLatin1(vendor);
    int major;
    int minor;
    int micro;
    if(sscanf(vendor, "Intel i965 driver - %d.%d.%d", &major, &minor, &micro) == 3)
    {
        if ((major > 1) || (major == 1 && minor > 0) || (major == 1 && minor == 0 && micro >= 17))
        {
            LOG(VB_GENERAL, LOG_INFO, "Deinterlacing support available in driver");
            m_supportedProperties << TorcPlayer::BasicDeinterlacing;
        }
    }

    // vendor
    if (vendorstr.contains("intel", Qt::CaseInsensitive) || vendorstr.contains("i965", Qt::CaseInsensitive))
    {
        m_vendor = INTEL;
    }
    else if (vendorstr.contains("nvidia", Qt::CaseInsensitive) || vendorstr.contains("vdpau", Qt::CaseInsensitive))
    {
        m_vendor = NVIDIA;
        // there's no mechanism to enable anything other than basic (bob)
        m_supportedProperties << TorcPlayer::BasicDeinterlacing;
    }
    else if (vendorstr.contains("xvba", Qt::CaseInsensitive))
    {
        m_vendor = AMD;
    }

    // sundry display attribute support
    int count = 0;
    VADisplayAttribute *attributes = new VADisplayAttribute[vaMaxNumDisplayAttributes(m_vaDisplay)];
    status = vaQueryDisplayAttributes(m_vaDisplay, attributes, &count);
    if (status == VA_STATUS_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("%1 supported display attributes").arg(count));
        for (int i = 0; i < count; ++i)
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Attribute: %1 (Set: %2 Get: %3) Current: %4 Min: %5 Max: %6")
                .arg(attributes[i].type)
                .arg(attributes[i].flags & VA_DISPLAY_ATTRIB_SETTABLE ? "Yes" : "No")
                .arg(attributes[i].flags & VA_DISPLAY_ATTRIB_GETTABLE ? "Yes" : "No")
                .arg(attributes[i].value)
                .arg(attributes[i].min_value)
                .arg(attributes[i].max_value));

            if (!(attributes[i].flags & VA_DISPLAY_ATTRIB_SETTABLE))
                continue;

            switch (attributes[i].type)
            {
                case VADisplayAttribBrightness:
                    m_supportedProperties << TorcPlayer::Brightness;
                    m_supportedAttributes << attributes[i];
                    break;
                case VADisplayAttribContrast:
                    m_supportedProperties << TorcPlayer::Contrast;
                    m_supportedAttributes << attributes[i];
                    break;
                case VADisplayAttribSaturation:
                    m_supportedProperties << TorcPlayer::Saturation;
                    m_supportedAttributes << attributes[i];
                    break;
                case VADisplayAttribHue:
                    m_supportedProperties << TorcPlayer::Hue;
                    m_supportedAttributes << attributes[i];
                    break;
                default:
                    break;
            }
        }
    }
    else
    {
        VA_ERROR(status, "Failed to retrieve display attributes");
    }
    delete [] attributes;

    m_state = Context;
    LOG(VB_GENERAL, LOG_INFO, QString("Created VAAPI context for %1 %2x%3 buffers").arg(m_numSurfaces).arg(m_avContext->width).arg(m_avContext->height));
    return true;
}

vaapi_context* VideoVAAPI::GetVAAPIContext(void)
{
    if (m_state == Context && m_vaapiContext)
        return m_vaapiContext;

    return NULL;
}

VAAPISurface* VideoVAAPI::GetNextSurface(void)
{
    for (int i = 0; i < m_numSurfaces; ++i)
    {
        if (m_surfaceData[i].m_allocated == 0)
        {
            m_surfaceData[i].m_allocated = 1;
            return &m_surfaceData[i];
        }
    }

    return NULL;
}

VideoVAAPI::Vendor VideoVAAPI::GetVendor(void)
{
    return m_vendor;
}

bool VideoVAAPI::CopySurfaceToTexture(VideoFrame *Frame, VAAPISurface *Surface,
                                      GLTexture *Texture, VideoColourSpace *ColourSpace)
{
    if (m_state == Context && m_opengl && Surface && Texture)
    {
        if (m_surfaceTexture != Texture && m_glxSurface)
        {
            QMutexLocker locker(m_lock);

            VAStatus status = VAAPIGLX.m_destroySurfaceGLX(m_vaDisplay, m_glxSurface);
            if (status != VA_STATUS_SUCCESS)
                VA_ERROR(status, "Failed to destroy GLX surface");

            m_glxSurface = NULL;
        }

        m_surfaceTexture = Texture;

        if (!m_glxSurface)
        {
            QMutexLocker locker(m_lock);

            VAStatus status = VAAPIGLX.m_createSurfaceGLX(m_vaDisplay, Texture->m_type, Texture->m_val, &m_glxSurface);
            if (status != VA_STATUS_SUCCESS || !m_glxSurface)
            {
                VA_ERROR(status, "Failed to create GLX surface");
                return false;
            }
        }

        // deinterlacing
        int flags = VA_FRAME_PICTURE;
        if (m_supportedProperties.contains(TorcPlayer::BasicDeinterlacing) && (Frame->m_field != VideoFrame::Frame))
            flags = Frame->m_field == VideoFrame::TopField ? VA_TOP_FIELD : VA_BOTTOM_FIELD;

        // colourspace - NB no support for full range YUV or studio levels
        switch (ColourSpace->GetColourSpace())
        {
            case AVCOL_SPC_SMPTE240M:
                flags |= VA_SRC_SMPTE_240;
                break;
            case AVCOL_SPC_BT709:
                flags |= VA_SRC_BT709;
                break;
            case AVCOL_SPC_RGB: // ?
            case AVCOL_SPC_BT470BG:
            case AVCOL_SPC_SMPTE170M:
            case AVCOL_SPC_UNSPECIFIED:
            case AVCOL_SPC_FCC:
            case AVCOL_SPC_YCOCG:
            default:
                flags |= VA_SRC_BT601;
        }

        if (!m_supportedAttributes.isEmpty() && ColourSpace->HasChanged())
        {
            for (int i = 0; i < m_supportedAttributes.size(); ++i)
            {
                int newvalue = -1;

                if (m_supportedAttributes.at(i).type == VADisplayAttribBrightness)
                    newvalue = ColourSpace->GetProperty(TorcPlayer::Brightness).toInt();
                else if (m_supportedAttributes.at(i).type == VADisplayAttribContrast)
                    newvalue = ColourSpace->GetProperty(TorcPlayer::Contrast).toInt();
                else if (m_supportedAttributes.at(i).type == VADisplayAttribSaturation)
                    newvalue = ColourSpace->GetProperty(TorcPlayer::Saturation).toInt();
                else if (m_supportedAttributes.at(i).type == VADisplayAttribHue)
                    newvalue = ColourSpace->GetProperty(TorcPlayer::Hue).toInt();

                if (newvalue >= 0)
                {
                    newvalue = m_supportedAttributes.at(i).min_value + (int)(((float)(newvalue % 100) / 100.0) *
                                                                       (m_supportedAttributes.at(i).max_value - m_supportedAttributes.at(i).min_value));

                }
            }

            VAStatus status = vaSetDisplayAttributes(m_vaDisplay, m_supportedAttributes.data(), m_supportedAttributes.size());

            if (status != VA_STATUS_SUCCESS)
                VA_ERROR(status, "Failed to set display attributes");

            ColourSpace->SetChanged(false);
        }

        m_lock->lock();
        VAStatus status = VAAPIGLX.m_copySurfaceGLX(m_vaDisplay, m_glxSurface, Surface->m_id, flags);
        m_lock->unlock();

        if (status != VA_STATUS_SUCCESS)
        {
            VA_ERROR(status, "Failed to copy surface to GLX");
            return false;
        }

        return true;
    }

    return false;
}

QSet<TorcPlayer::PlayerProperty> VideoVAAPI::GetSupportedProperties(void)
{
    return m_supportedProperties;
}

class VAAPIFactory : public AccelerationFactory
{
  public:
    AVCodec* SelectAVCodec(AVCodecContext *Context)
    {
        return NULL;
    }

    bool InitialiseDecoder(AVCodecContext *Context, AVPixelFormat Format)
    {
        if (!(VideoPlayer::gAllowGPUAcceleration && VideoPlayer::gAllowGPUAcceleration->IsActive() &&
            VideoPlayer::gAllowGPUAcceleration->GetValue().toBool()))
        {
            return false;
        }

        if (Context->hwaccel_context)
        {
            LOG(VB_GENERAL, LOG_ERR, "AVCodecContext already has a hardware acceleration context. Aborting");
            return false;
        }

        if (!VideoVAAPI::InitialiseDecoder(Context, Format))
            return false;

        AVPixelFormat oldformat = Context->pix_fmt;
        Context->pix_fmt = AV_PIX_FMT_VAAPI_VLD;
        VideoVAAPI* vaapivideo = VideoVAAPI::GetVideoVAAPI(Context, true);

        if (!vaapivideo)
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to create VAAPI video object");
            Context->pix_fmt = oldformat;
            return false;
        }

        vaapi_context* vaapicontext = vaapivideo->GetVAAPIContext();

        if (!vaapicontext)
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to retrieve VAAPI context");
            VideoVAAPI::DeinitialiseDecoder(Context);
            Context->pix_fmt = oldformat;
            return false;
        }

        Context->slice_flags     = SLICE_FLAG_CODED_ORDER | SLICE_FLAG_ALLOW_FIELD;
        Context->hwaccel_context = vaapicontext;
        return true;
    }

    void DeinitialiseDecoder(AVCodecContext *Context)
    {
        if (Context && Context->hwaccel_context && Context->pix_fmt == AV_PIX_FMT_VAAPI_VLD)
            VideoVAAPI::DeinitialiseDecoder(Context);
    }

    bool InitialiseBuffer(AVCodecContext *Context, AVFrame *Avframe, VideoFrame *Frame)
    {
        if (!Context || !Avframe || !Frame)
            return false;

        if (Context->pix_fmt != AV_PIX_FMT_VAAPI_VLD || Avframe->format != AV_PIX_FMT_VAAPI_VLD)
            return false;

        vaapi_context *vaapicontext = (vaapi_context*)Context->hwaccel_context;
        VAAPISurface  *vaapisurface = (VAAPISurface*)Frame->m_acceleratedBuffer;

        if (!vaapicontext)
        {
            LOG(VB_GENERAL, LOG_ERR, "No VAAPI hardware context");
            return false;
        }

        if (!vaapisurface)
        {
            VideoVAAPI* vaapivideo = VideoVAAPI::GetVideoVAAPI(Context, true);

            if (vaapivideo)
                vaapisurface = vaapivideo->GetNextSurface();

            if (!vaapisurface)
            {
                LOG(VB_GENERAL, LOG_ERR, "Failed to get next VAAPI video surface");
                return false;
            }

            Frame->m_acceleratedBuffer = vaapisurface;
        }

        vaapisurface->m_owner->Lock();
        VAStatus status = vaSyncSurface(vaapicontext->display, vaapisurface->m_id);
        vaapisurface->m_owner->Unlock();

        if (status != VA_STATUS_SUCCESS)
            VA_ERROR(status, "Failed to sync surface");

        Avframe->data[0]     = (unsigned char*)&vaapisurface;
        Avframe->data[1]     = NULL;
        Avframe->data[2]     = NULL;
        Avframe->data[3]     = (uint8_t*)vaapisurface->m_id;
        Avframe->linesize[0] = 0;
        Avframe->linesize[1] = 0;
        Avframe->linesize[2] = 0;
        Avframe->linesize[3] = 0;

        return true;
    }

    void DeinitialiseBuffer(AVCodecContext *Context, AVFrame *Avframe, VideoFrame *Frame)
    {
        (void)Context;
        (void)Avframe;
        (void)Frame;
    }

    void ConvertBuffer(AVFrame &Avframe, VideoFrame *Frame, SwsContext *&ConversionContext)
    {
        (void)Avframe;
        (void)Frame;
        (void)ConversionContext;
    }

    bool UpdateFrame(VideoFrame *Frame, VideoColourSpace *ColourSpace, void *Surface)
    {
        if (Frame && Surface && ColourSpace && Frame->m_pixelFormat == AV_PIX_FMT_VAAPI_VLD && Frame->m_acceleratedBuffer)
        {
            VAAPISurface *vaapisurface = (VAAPISurface*)Frame->m_acceleratedBuffer;
            GLTexture     *texture     = static_cast<GLTexture*>(Surface);

            if (vaapisurface)
            {
                VideoVAAPI *videovaapi = vaapisurface->m_owner;

                if (videovaapi)
                {
                    if (texture)
                        videovaapi->CopySurfaceToTexture(Frame, vaapisurface, texture, ColourSpace);

                    if (videovaapi->IsDeleting())
                    {
                        Frame->m_acceleratedBuffer = NULL;
                        videovaapi->Dereference();
                    }
                }
            }

            return true;
        }

        return false;
    }

    bool ReleaseFrame(VideoFrame *Frame)
    {
        if (Frame && Frame->m_pixelFormat == AV_PIX_FMT_VAAPI_VLD && Frame->m_acceleratedBuffer)
        {
            VAAPISurface *vaapisurface = (VAAPISurface*)Frame->m_acceleratedBuffer;
            if (vaapisurface)
            {
                VideoVAAPI *videovaapi = vaapisurface->m_owner;
                if (videovaapi)
                    videovaapi->Dereference();
                Frame->m_acceleratedBuffer = NULL;
            }

            return true;
        }

        return false;
    }

    bool MapFrame(VideoFrame *Frame, void *Surface)
    {
        (void)Frame;
        (void)Surface;
        return false;
    }

    bool UnmapFrame(VideoFrame *Frame, void *Surface)
    {
        (void)Frame;
        (void)Surface;
        return false;
    }

    bool NeedsCustomSurfaceFormat(VideoFrame *Frame, void *Format)
    {
        if (Frame && Format && Frame->m_pixelFormat == AV_PIX_FMT_VAAPI_VLD &&
            Frame->m_acceleratedBuffer && VideoVAAPI::VAAPIAvailable(true))
        {
            VAAPISurface *vaapi = (VAAPISurface*)Frame->m_acceleratedBuffer;
            if (vaapi && vaapi->m_owner)
            {
                VideoVAAPI::Vendor vendor = vaapi->m_owner->GetVendor();
                if (vendor == VideoVAAPI::INTEL)
                {
                    int *format = static_cast<int*>(Format);
                    if (format)
                    {
                        *format = GL_TEXTURE_2D;
                        return true;
                    }
                }
            }
        }

        return false;
    }

    bool SupportedProperties(VideoFrame *Frame, QSet<TorcPlayer::PlayerProperty> &Properties)
    {
        if (Frame && Frame->m_pixelFormat == AV_PIX_FMT_VAAPI_VLD &&
            Frame->m_acceleratedBuffer && VideoVAAPI::VAAPIAvailable(true))
        {
            VAAPISurface *vaapi = (VAAPISurface*)Frame->m_acceleratedBuffer;
            if (vaapi && vaapi->m_owner)
            {
                Properties.unite(vaapi->m_owner->GetSupportedProperties());
                return true;
            }
        }

        return false;
    }
} VAAPIFactory;
