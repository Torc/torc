/* Class VideoVDPAU
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
#include <QMutex>

// Torc
#include "torclogging.h"
#include "torcthread.h"
#include "videodecoder.h"
#include "videoplayer.h"
#include "videoframe.h"
#include "opengl/uiopenglwindow.h"
#include "platforms/nvctrl/uinvcontrol.h"
#include "videovdpau.h"

#include <GL/glx.h>

extern "C" {
#include "libavcodec/vdpau.h"
}

static QMutex* gVDPAULock = new QMutex(QMutex::Recursive);

typedef GLintptr        InteropSurface;
typedef void           (APIENTRY * TORC_INITNV)     (const void*, const void*);
typedef void           (APIENTRY * TORC_FININV)     (void);
typedef InteropSurface (APIENTRY * TORC_REGOUTSURF) (const void*, GLenum, GLsizei, const uint*);
typedef GLboolean      (APIENTRY * TORC_ISSURFACE)  (InteropSurface);
typedef void           (APIENTRY * TORC_UNREGSURF)  (InteropSurface);
typedef void           (APIENTRY * TORC_SURFACCESS) (InteropSurface, GLenum);
typedef void           (APIENTRY * TORC_MAPSURF)    (GLsizei, const InteropSurface *);
typedef void           (APIENTRY * TORC_UNMAPSURF)  (GLsizei, const InteropSurface *);

class NVInterop
{
  public:
    NVInterop()
      : m_valid(false),
        m_init(NULL),
        m_fini(NULL),
        m_registerOutputSurface(NULL),
        m_isSurface(NULL),
        m_unregisterSurface(NULL),
        m_surfaceAccess(NULL),
        m_mapSurface(NULL),
        m_unmapSurface(NULL)
    {
        QByteArray extensions((const char*)(glGetString(GL_EXTENSIONS)));

        if (extensions.contains("GL_NV_vdpau_interop"))
        {
            m_init                  = (TORC_INITNV)     UIOpenGLWindow::GetProcAddress("glVDPAUInitNV");
            m_fini                  = (TORC_FININV)     UIOpenGLWindow::GetProcAddress("glVDPAUFiniNV");
            m_registerOutputSurface = (TORC_REGOUTSURF) UIOpenGLWindow::GetProcAddress("glVDPAURegisterOutputSurfaceNV");
            m_isSurface             = (TORC_ISSURFACE)  UIOpenGLWindow::GetProcAddress("glVDPAUIsSurfaceNV");
            m_unregisterSurface     = (TORC_UNREGSURF)  UIOpenGLWindow::GetProcAddress("glVDPAUUnregisterSurfaceNV");
            m_surfaceAccess         = (TORC_SURFACCESS) UIOpenGLWindow::GetProcAddress("glVDPAUSurfaceAccessNV");
            m_mapSurface            = (TORC_MAPSURF)    UIOpenGLWindow::GetProcAddress("glVDPAUMapSurfaceNV");
            m_unmapSurface          = (TORC_UNMAPSURF)  UIOpenGLWindow::GetProcAddress("glVDPAUUnmapSurfaceNV");
        }

        m_valid = m_init && m_fini && m_registerOutputSurface && m_unregisterSurface &&
                  m_isSurface && m_surfaceAccess && m_mapSurface && m_unmapSurface;
    }

    bool IsValid(void)
    {
        return m_valid;
    }

    bool             m_valid;
    TORC_INITNV      m_init;
    TORC_FININV      m_fini;
    TORC_REGOUTSURF  m_registerOutputSurface;
    TORC_ISSURFACE   m_isSurface;
    TORC_UNREGSURF   m_unregisterSurface;
    TORC_SURFACCESS  m_surfaceAccess;
    TORC_MAPSURF     m_mapSurface;
    TORC_UNMAPSURF   m_unmapSurface;
};

QString ProfileToString(VdpDecoderProfile Profile)
{
    switch (Profile)
    {
        case VDP_DECODER_PROFILE_MPEG1:              return QString("MPEG1");
        case VDP_DECODER_PROFILE_MPEG2_SIMPLE:       return QString("MPEG2Simple");
        case VDP_DECODER_PROFILE_MPEG2_MAIN:         return QString("MPEG2Main");
        case VDP_DECODER_PROFILE_H264_BASELINE:      return QString("H264Baseline");
        case VDP_DECODER_PROFILE_H264_MAIN:          return QString("H264Main");
        case VDP_DECODER_PROFILE_H264_HIGH:          return QString("H264High");
        case VDP_DECODER_PROFILE_VC1_SIMPLE:         return QString("VC1Simple");
        case VDP_DECODER_PROFILE_VC1_MAIN:           return QString("VC1Main");
        case VDP_DECODER_PROFILE_VC1_ADVANCED:       return QString("VC1Advanced");
        case VDP_DECODER_PROFILE_MPEG4_PART2_SP:     return QString("MPEG4Part2SP");
        case VDP_DECODER_PROFILE_MPEG4_PART2_ASP:    return QString("MPEG4Part2ASP");
        case VDP_DECODER_PROFILE_DIVX4_QMOBILE:      return QString("DIVX4QMobile");
        case VDP_DECODER_PROFILE_DIVX4_MOBILE:       return QString("DIVX4Mobile");
        case VDP_DECODER_PROFILE_DIVX4_HOME_THEATER: return QString("DIVX4HomeTheater");
        case VDP_DECODER_PROFILE_DIVX4_HD_1080P:     return QString("DIVX4HD1080P");
        case VDP_DECODER_PROFILE_DIVX5_QMOBILE:      return QString("DIVX5QMobile");
        case VDP_DECODER_PROFILE_DIVX5_MOBILE:       return QString("DIVX5Mobile");
        case VDP_DECODER_PROFILE_DIVX5_HOME_THEATER: return QString("DIVX5HomeTheater");
        case VDP_DECODER_PROFILE_DIVX5_HD_1080P:     return QString("DIVX5HD1080P");
    }

    return QString("Unknown");
}

bool CodecToProfile(AVCodecID Codec, VdpDecoderProfile &Profile)
{
    switch (Codec)
    {
        case AV_CODEC_ID_MPEG1VIDEO:
            Profile = VDP_DECODER_PROFILE_MPEG1;
            return true;
        case AV_CODEC_ID_MPEG2VIDEO:
            Profile = VDP_DECODER_PROFILE_MPEG2_MAIN;
            return true;
        case AV_CODEC_ID_MPEG4:
            Profile = VDP_DECODER_PROFILE_MPEG4_PART2_ASP;
            return true;
        case AV_CODEC_ID_H264:
            Profile = VDP_DECODER_PROFILE_H264_HIGH;
            return true;
        case AV_CODEC_ID_WMV3:
            Profile = VDP_DECODER_PROFILE_VC1_MAIN;
            return true;
        case AV_CODEC_ID_VC1:
            Profile = VDP_DECODER_PROFILE_VC1_ADVANCED;
            return true;
        default:
            break;
    }

    return false;
}

#define VDPAU_ERROR(Error,Message) LOG(VB_GENERAL, LOG_ERR, QString("%1 (%2)").arg(Message).arg(m_getErrorString(Error)));
#define PREEMPTION_CHECK while (m_preempted.fetchAndAddOrdered(0) > 0) { HandlePreemption(); }

VDPAUDecoderCapability::VDPAUDecoderCapability()
  : m_profile(0),
    m_maxLevel(0),
    m_maxMacroblocks(0),
    m_minWidth(0),
    m_minHeight(0),
    m_maxWidth(0),
    m_maxHeight(0)
{
}

VDPAUDecoderCapability::VDPAUDecoderCapability(VdpDecoderProfile Profile, uint32_t MaxLevel, uint32_t MaxMacroblocks, uint32_t MaxWidth, uint32_t MaxHeight)
  : m_profile(Profile),
    m_maxLevel(MaxLevel),
    m_maxMacroblocks(MaxMacroblocks),
    m_minWidth(48),
    m_minHeight(48),
    m_maxWidth(MaxWidth),
    m_maxHeight(MaxHeight)
{
}

bool VDPAUDecoderCapability::IsSupported(uint32_t Width, uint32_t Height)
{
    if (Width < m_minWidth || Height < m_minHeight || Width > m_maxWidth || Height > m_maxHeight)
        return false;

    int mbwidth  = ceil((double)Width  / 16.0f);
    int mbheight = ceil((double)Height / 16.0f);

    if ((mbwidth * mbheight) > (int)m_maxMacroblocks)
        return false;

    return true;
}

void Decode(struct AVCodecContext *Context, const AVFrame *Avframe, int*, int, int, int)
{
    if (!Avframe || !Context)
        return;

    VideoVDPAU* vdpau = (VideoVDPAU*)Context->hwaccel_context;
    if (vdpau)
        vdpau->Decode(Avframe);
    else
        LOG(VB_GENERAL, LOG_ERR, "Video decoding error");
}

bool VideoVDPAU::VDPAUAvailable(void)
{
    QMutexLocker locker(gVDPAULock);

    static bool available = false;
    static bool checked   = false;

    if (checked)
        return available;

    checked = true;

    VideoVDPAU* vdpau = new VideoVDPAU(NULL);
    if (vdpau)
    {
        if (vdpau->CreateTestVDPAU())
        {
            LOG(VB_GENERAL, LOG_INFO, "VDPAU hardware decoding available");
            available = true;
        }

        vdpau->Dereference();
    }

    return available;
}

bool VideoVDPAU::CheckHardwareSupport(AVCodecContext *Context)
{
    if (!Context || !VideoVDPAU::VDPAUAvailable())
        return false;

    bool supported = false;

    VideoVDPAU *vdpau = new VideoVDPAU(NULL);
    if (vdpau)
    {
        if (vdpau->CreateTestVDPAU())
            supported = vdpau->IsFormatSupported(Context);

        vdpau->Dereference();
    }

    return supported;
}

VideoVDPAU* VideoVDPAU::Create(AVCodecContext *Context)
{
    if (Context)
    {
        VideoVDPAU* vdpau = new VideoVDPAU(Context);

        if (vdpau)
        {
            if (vdpau->CreateFullVDPAU())
                return vdpau;

            vdpau->Dereference();
        }
    }

    return NULL;
}

static void PreemptionCallback(VdpDevice Device, void* Object)
{
    (void)Device;

    VideoVDPAU *object = (VideoVDPAU*)Object;
    if (object)
        object->Preempted();
}

static const char* GetErrorString(VdpStatus Error)
{
    (void)Error;
    static const char dummy[] = "Unknown";
    return &dummy[0];
}

/*! \class VideoVDPAU
 *
 * \todo Preemption handling
 * \todo Video chroma format checks
 * \todo Decoder profile level checks
 * \todo Frame mixing and rendering in UpdateFrame
 * \todo Surface mapping/unmapping for actual display
*/

VideoVDPAU::VideoVDPAU(AVCodecContext *Context)
  : TorcReferenceCounter(),
    m_state(Created),
    m_callbackLock(new QMutex()),
    m_avContext(Context),
    m_nvidiaInterop(NULL),
    m_display(NULL),
    m_createdDisplay(false),
    m_device(0),
    m_featureSet(Set_Unknown),
    m_decoder(0),
    m_getErrorString(&GetErrorString)
{
    ResetProcs();
}

VideoVDPAU::~VideoVDPAU()
{
    m_state = Deleted;

    TearDown();

    delete m_callbackLock;

    LOG(VB_GENERAL, LOG_INFO, "VideoVDPAU destroyed");
}

void VideoVDPAU::VDPAUUICallback(void* Object, int Parameter)
{
    VideoVDPAU *vdpau = static_cast<VideoVDPAU*>(Object);

    if (!vdpau)
        return;

    switch ((VideoVDPAU::CallbackAction)Parameter)
    {
        case CreateFull:
            if (!vdpau->CreateFullVDPAU())
                LOG(VB_GENERAL, LOG_ERR, "Failed to initialise VDPAU via callback");
            vdpau->Wake();
            break;
        case Destroy:
            vdpau->Dereference();
            break;
        case Recreate:
            if (!vdpau->HandlePreemption())
                LOG(VB_GENERAL, LOG_ERR, "Failed to handle preemption via callback");
            vdpau->Wake();
            break;
        default:
            break;
    }
}

bool VideoVDPAU::CreateTestVDPAU(void)
{
    TorcReferenceLocker locker(this);

    // create a connection to the X server
    m_createdDisplay = true;
    m_display = XOpenDisplay(NULL);

    if (m_display && CreateDevice() && GetProcs() && CheckHardwareSupport())
    {
        m_state = Test;
        return true;
    }

    if (!m_display)
        LOG(VB_GENERAL, LOG_ERR, "Failed to open X connection");

    m_state = Errored;
    return false;
}

bool VideoVDPAU::CreateFullVDPAU(void)
{
    TorcReferenceLocker rlocker(this);

    // this must be completed in the main UI thread
    if (!TorcThread::IsMainThread())
    {
        UIOpenGLWindow *window = static_cast<UIOpenGLWindow*>(gLocalContext->GetUIObject());
        if (window)
        {
            m_callbackLock->lock();
            window->RegisterCallback(VDPAUUICallback, this, CreateFull);
            m_callbackWait.wait(m_callbackLock);
            m_callbackLock->unlock();
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to find UI window");
            return false;
        }

        return m_state == Full;
    }

    m_display = glXGetCurrentDisplay();

    if (m_avContext && m_display && CreateDevice() && GetProcs() && CheckHardwareSupport() &&
        CreateDecoder() && CreateVideoSurfaces() && RegisterCallback(true))
    {
        m_state = Full;
        return true;
    }

    if (!m_display)
        LOG(VB_GENERAL, LOG_ERR, "Failed to get X connection");

    m_state = Errored;
    return false;
}

void VideoVDPAU::Preempted(void)
{
    LOG(VB_GENERAL, LOG_INFO, "Display preempted");
    m_preempted.ref();
}

void VideoVDPAU::Decode(const AVFrame *Avframe)
{
    PREEMPTION_CHECK

    if (!Avframe || !m_decoder || !m_decoderRender ||
        !(Avframe->format == AV_PIX_FMT_VDPAU_H264  || Avframe->format == AV_PIX_FMT_VDPAU_MPEG1 ||
          Avframe->format == AV_PIX_FMT_VDPAU_MPEG2 || Avframe->format == AV_PIX_FMT_VDPAU_MPEG4 ||
          Avframe->format == AV_PIX_FMT_VDPAU_VC1   || Avframe->format == AV_PIX_FMT_VDPAU_WMV3))
    {
        LOG(VB_GENERAL, LOG_ERR, "Decode called with invalid frame");
        return;
    }

    struct vdpau_render_state *render = (struct vdpau_render_state *)Avframe->data[0];
    if (!render)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to retrieve surface");
        return;
    }

    VdpStatus status = m_decoderRender(m_decoder, render->surface,
                                       (VdpPictureInfo const *)&(render->info),
                                       render->bitstream_buffers_used,
                                       render->bitstream_buffers);

    if (status != VDP_STATUS_OK)
        VDPAU_ERROR(status, "Video decoding error");
}

struct vdpau_render_state* VideoVDPAU::GetSurface(void)
{
    PREEMPTION_CHECK

    if (m_createdVideoSurfaces.isEmpty() || m_state == Errored)
        return NULL;

    struct vdpau_render_state* vdpau = m_createdVideoSurfaces.takeFirst();
    m_allocatedVideoSurfaces.push_front(vdpau);
    return vdpau;
}

void VideoVDPAU::Wake(void)
{
    m_callbackWait.wakeAll();
}

bool VideoVDPAU::Dereference(void)
{
    if (!IsShared() && !TorcThread::IsMainThread())
    {
        // destroy from the UI/OpenGL thread. No need to wait for completion
        UIOpenGLWindow *window = static_cast<UIOpenGLWindow*>(gLocalContext->GetUIObject());
        if (window)
            window->RegisterCallback(VDPAUUICallback, this, Destroy);
        return false;
    }

    return TorcReferenceCounter::DownRef();
}

void VideoVDPAU::SetDeleting(void)
{
    if (m_state != Errored)
        m_state = Deleting;
}

bool VideoVDPAU::IsDeletingOrErrored(void)
{
    return m_state == Errored || m_state == Deleting;
}

bool VideoVDPAU::IsFormatSupported(AVCodecContext *Context)
{
    PREEMPTION_CHECK

    if (!Context || m_state == Errored || m_decoderCapabilities.isEmpty())
        return false;

    // is this a VDPAU supported profile
    VdpDecoderProfile profile = 0;
    if (!CodecToProfile(Context->codec_id, profile))
        return false;

    // does this implementation support this profile
    if (!m_decoderCapabilities.contains(profile))
        return false;

    // does this implementation support this size
    if (!m_decoderCapabilities[profile].IsSupported(Context->width, Context->height))
        return false;

    // check NVIDIA H264 limitations on older hardware
    if ((m_featureSet == Set_A1 || m_featureSet == Set_B1) && Context->codec_id == AV_CODEC_ID_H264)
    {
        if ((Context->width >= 769  && Context->width <= 784)  ||
            (Context->width >= 849  && Context->width <= 864)  ||
            (Context->width >= 929  && Context->width <= 944)  ||
            (Context->width >= 1009 && Context->width <= 1024) ||
            (Context->width >= 1793 && Context->width <= 1808) ||
            (Context->width >= 1873 && Context->width <= 1888) ||
            (Context->width >= 1953 && Context->width <= 1968) ||
            (Context->width >= 2033 && Context->width <= 2048))
        {
            if (!m_decoderCreate || !m_device || !m_decoderDestroy)
                return false;

            VdpDecoder decoder = 0;
            VdpStatus status = m_decoderCreate(m_device, profile, Context->width, Context->height, Context->refs, &decoder);
            if (status != VDP_STATUS_OK)
            {
                VDPAU_ERROR(status, "VDPAU does not support this video stream's width");
                return false;
            }

            m_decoderDestroy(decoder);
        }
    }

    return true;
}

bool VideoVDPAU::CreateDevice(void)
{
    if (!m_display || m_state == Errored)
        return false;

    VdpStatus status = vdp_device_create_x11(m_display, DefaultScreen(m_display), &m_device, &m_getProcAddress);

    if (status != VDP_STATUS_OK)
    {
        VDPAU_ERROR(status, "Failed to create device");
        return false;
    }

    status = m_getProcAddress(m_device, VDP_FUNC_ID_GET_ERROR_STRING, (void**)&m_getErrorString);

    if (status != VDP_STATUS_OK)
        m_getErrorString = &GetErrorString;

    return true;
}

bool VideoVDPAU::CreateDecoder(void)
{
    if (!m_avContext || !m_decoderCreate || !m_device || m_state == Errored)
        return false;

    VdpDecoderProfile profile = 0;
    if (!CodecToProfile(m_avContext->codec_id, profile))
        return false;

    VdpStatus status = m_decoderCreate(m_device, profile, m_avContext->width, m_avContext->height, m_avContext->refs, &m_decoder);
    if (status != VDP_STATUS_OK)
    {
        VDPAU_ERROR(status, "Failed to create decoder");
        return false;
    }

    return true;
}

bool VideoVDPAU::CreateVideoSurfaces(void)
{
    if (!m_device || !m_videoSurfaceCreate || !m_avContext || m_state == Errored)
        return false;

    // we need to handle initial creation as well as preemption

    // reset any existing surfaces
    foreach (struct vdpau_render_state* state, m_createdVideoSurfaces)
        state->surface = 0;
    foreach (struct vdpau_render_state* state, m_allocatedVideoSurfaces)
        state->surface = 0;

    int required = MIN_VIDEO_BUFFERS_FOR_DISPLAY + std::max(MIN_VIDEO_BUFFERS_FOR_DECODE, m_avContext->refs);

    // create the structures
    while ((m_createdVideoSurfaces.size() + m_allocatedVideoSurfaces.size()) < required)
    {
        struct vdpau_render_state* state = new vdpau_render_state();
        m_createdVideoSurfaces << state;
        // increase the ref count for each frame
        UpRef();
    }

    // create the surfaces
    foreach (struct vdpau_render_state* state, m_createdVideoSurfaces)
    {
        VdpOutputSurface surface = 0;
        VdpStatus status = m_videoSurfaceCreate(m_device, VDP_CHROMA_TYPE_420, m_avContext->width, m_avContext->height, &surface);
        if (status == VDP_STATUS_OK)
            state->surface = surface;
        else
            VDPAU_ERROR(status, "Error creating video surface");
    }

    foreach (struct vdpau_render_state* state, m_allocatedVideoSurfaces)
    {
        VdpOutputSurface surface = 0;
        VdpStatus status = m_videoSurfaceCreate(m_device, VDP_CHROMA_TYPE_420, m_avContext->width, m_avContext->height, &surface);
        if (status == VDP_STATUS_OK)
            state->surface = surface;
        else
            VDPAU_ERROR(status, "Error creating video surface");
    }

    LOG(VB_GENERAL, LOG_INFO, QString("(Re-)Created %1 VDPAU video surfaces").arg(required));
    return true;
}

#define GET_PROC(ID, FUNCTION) \
    status = m_getProcAddress(m_device, ID, (void **)&FUNCTION); \
    if (status != VDP_STATUS_OK) \
    { \
        valid = false; \
        VDPAU_ERROR(status, "Failed to get VDPAU function"); \
    }

bool VideoVDPAU::GetProcs(void)
{
    if (!m_getProcAddress || !m_device || m_state == Errored)
        return false;

    ResetProcs();

    bool valid = true;
    VdpStatus status;

    GET_PROC(VDP_FUNC_ID_DEVICE_DESTROY,               m_deviceDestroy);
    GET_PROC(VDP_FUNC_ID_PREEMPTION_CALLBACK_REGISTER, m_preemptionCallbackRegister);
    GET_PROC(VDP_FUNC_ID_DECODER_CREATE,               m_decoderCreate);
    GET_PROC(VDP_FUNC_ID_DECODER_DESTROY,              m_decoderDestroy);
    GET_PROC(VDP_FUNC_ID_DECODER_RENDER,               m_decoderRender);
    GET_PROC(VDP_FUNC_ID_DECODER_QUERY_CAPABILITIES,   m_decoderQueryCapabilities);
    GET_PROC(VDP_FUNC_ID_VIDEO_SURFACE_CREATE,         m_videoSurfaceCreate);
    GET_PROC(VDP_FUNC_ID_VIDEO_SURFACE_DESTROY,        m_videoSurfaceDestroy);

    if (!valid)
        LOG(VB_GENERAL, LOG_ERR, "Failed to link to VDPAU library");

    // not critical
    m_getProcAddress(m_device, VDP_FUNC_ID_GET_API_VERSION, (void **)&m_getApiVersion);
    m_getProcAddress(m_device, VDP_FUNC_ID_GET_INFORMATION_STRING, (void **)&m_getInformationString);

    return valid;
}

bool VideoVDPAU::RegisterCallback(bool Register)
{
    if (m_preemptionCallbackRegister && m_device)
    {
        VdpStatus status = m_preemptionCallbackRegister(m_device, Register ? PreemptionCallback : NULL, (void*)this);
        if (status == VDP_STATUS_OK)
            return true;

        VDPAU_ERROR(status, "Failed to register callback");
    }

    return false;
}

bool VideoVDPAU::CheckHardwareSupport(void)
{
    m_decoderCapabilities.clear();

    if (!m_display || !m_decoderQueryCapabilities || !m_device || m_state == Errored)
        return false;

    if (UINVControl::NVControlAvailable(m_display))
        m_featureSet = GetNVIDIAFeatureSet(UINVControl::GetPCIID(m_display, DefaultScreen(m_display)) & 0xffff);

    VdpStatus status;

    static VdpDecoderProfile profiles[] = { VDP_DECODER_PROFILE_MPEG1,
                                            VDP_DECODER_PROFILE_MPEG2_SIMPLE,
                                            VDP_DECODER_PROFILE_MPEG2_MAIN,
                                            VDP_DECODER_PROFILE_H264_BASELINE,
                                            VDP_DECODER_PROFILE_H264_MAIN,
                                            VDP_DECODER_PROFILE_H264_HIGH,
                                            VDP_DECODER_PROFILE_VC1_SIMPLE,
                                            VDP_DECODER_PROFILE_VC1_MAIN,
                                            VDP_DECODER_PROFILE_VC1_ADVANCED,
                                            VDP_DECODER_PROFILE_MPEG4_PART2_SP,
                                            VDP_DECODER_PROFILE_MPEG4_PART2_ASP,
                                            VDP_DECODER_PROFILE_DIVX4_QMOBILE,
                                            VDP_DECODER_PROFILE_DIVX4_MOBILE,
                                            VDP_DECODER_PROFILE_DIVX4_HOME_THEATER,
                                            VDP_DECODER_PROFILE_DIVX4_HD_1080P,
                                            VDP_DECODER_PROFILE_DIVX5_QMOBILE,
                                            VDP_DECODER_PROFILE_DIVX5_MOBILE,
                                            VDP_DECODER_PROFILE_DIVX5_HOME_THEATER,
                                            VDP_DECODER_PROFILE_DIVX5_HD_1080P };

    static bool debugged = false;

    for (uint i = 0; i < sizeof(profiles) / sizeof(VdpDecoderProfile); ++i)
    {
        VdpBool  supported = 0;
        uint32_t profile = profiles[i];
        uint32_t level = 0;
        uint32_t macroblocks = 0;
        uint32_t width = 0;
        uint32_t height = 0;

        status = m_decoderQueryCapabilities(m_device, profile, &supported, &level, &macroblocks, &width, &height);
        if (status == VDP_STATUS_OK && supported)
        {
            if (!debugged)
            {
                LOG(VB_GENERAL, LOG_INFO, QString("VDPAU decoder support: Profile %1 Level %2 Macroblocks %3 Size %4x%5")
                        .arg(ProfileToString(profile)).arg(level).arg(macroblocks).arg(width).arg(height));
            }

            m_decoderCapabilities.insert(profile, VDPAUDecoderCapability(profile, level, macroblocks, width, height));
        }
    }

    if (!debugged)
    {
        if (m_getApiVersion)
        {
            uint version;
            m_getApiVersion(&version);
            LOG(VB_GENERAL, LOG_INFO, QString("VDPAU Version '%1'").arg(version));
        }

        if (m_getInformationString)
        {
            const char * info;
            m_getInformationString(&info);
            LOG(VB_GENERAL, LOG_INFO, QString("VDPAU Information '%2'").arg(info));
        }
    }

    debugged = true;
    return true;
}

void VideoVDPAU::TearDown(void)
{
    // deregister callback
    RegisterCallback(false);

    // delete video surfaces
    DestroyVideoSurfaces();

    // delete decoder
    DestroyDecoder();

    // delete device
    DestroyDevice();

    // delete interop hooks
    delete m_nvidiaInterop;
    m_nvidiaInterop = NULL;

    // and vdpau hooks
    ResetProcs();

    // close display if necessary
    if (m_display && m_createdDisplay)
        XCloseDisplay(m_display);
    m_display = NULL;
    m_createdDisplay = false;
}

void VideoVDPAU::DestroyDevice(void)
{
    m_getErrorString = NULL;
    m_getProcAddress = NULL;
    if (m_deviceDestroy && m_device)
    {
        m_deviceDestroy(m_device);
        m_device = 0;
    }
}

void VideoVDPAU::DestroyDecoder(void)
{
    if (m_decoderDestroy && m_decoder)
        m_decoderDestroy(m_decoder);
}

void VideoVDPAU::DestroyVideoSurfaces(void)
{
    if (!m_videoSurfaceDestroy)
        return;

    m_createdVideoSurfaces.append(m_allocatedVideoSurfaces);
    m_allocatedVideoSurfaces.clear();

    while (!m_createdVideoSurfaces.isEmpty())
    {
        struct vdpau_render_state* render = m_createdVideoSurfaces.takeFirst();
        VdpStatus status = m_videoSurfaceDestroy(render->surface);
        if (status != VDP_STATUS_OK)
            VDPAU_ERROR(status, "Error deleting video surface");
        delete render;
    }

    m_createdVideoSurfaces.clear();
}

void VideoVDPAU::ResetProcs(void)
{
    m_getApiVersion              = NULL;
    m_getInformationString       = NULL;
    m_deviceDestroy              = NULL;
    m_preemptionCallbackRegister = NULL;
    m_decoderCreate              = NULL;
    m_decoderDestroy             = NULL;
    m_decoderRender              = NULL;
    m_decoderQueryCapabilities   = NULL;
    m_videoSurfaceCreate         = NULL;
    m_videoSurfaceDestroy        = NULL;
}

bool VideoVDPAU::HandlePreemption(void)
{
    if (m_preempted.fetchAndAddOrdered(0) < 1)
        return true;

    if (!TorcThread::IsMainThread())
    {
        UIOpenGLWindow *window = static_cast<UIOpenGLWindow*>(gLocalContext->GetUIObject());
        if (window)
        {
            m_callbackLock->lock();
            window->RegisterCallback(VDPAUUICallback, this, Recreate);
            m_callbackWait.wait(m_callbackLock);
            m_callbackLock->unlock();
        }

        if (m_preempted.fetchAndAddOrdered(0) > 0)
        {
            LOG(VB_GENERAL, LOG_ERR, "Preemption recovery failed");
            m_state = Errored;
        }

        return m_state != Errored;
    }

    LOG(VB_GENERAL, LOG_INFO, "Starting preemption recovery");
    bool result = false;

    ResetProcs();
    m_device = 0;
    m_decoder = 0;

    if (m_state == Test)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Test VDPAU object preempted - this shouldn't happen!");
        if (m_display)
            XCloseDisplay(m_display);
        m_display = NULL;
        result = CreateTestVDPAU();
    }
    else
    {
        result = CreateFullVDPAU();
    }

    m_preempted.deref();

    LOG(VB_GENERAL, LOG_INFO, "Preemption recovery finished");

    return result;
}

class VDPAUFactory : public AccelerationFactory
{
    AVCodec* SelectAVCodec(AVCodecContext *Context)
    {
        if (Context && VideoPlayer::gAllowGPUAcceleration && VideoPlayer::gAllowGPUAcceleration->IsActive() &&
            VideoPlayer::gAllowGPUAcceleration->GetValue().toBool() && VideoVDPAU::VDPAUAvailable())
        {
            AVCodec* next = NULL;
            while ((next = av_codec_next(next)))
            {
                if (next->id == Context->codec_id && (next->capabilities & CODEC_CAP_HWACCEL_VDPAU))
                {
                    if (!VideoVDPAU::CheckHardwareSupport(Context))
                        return NULL;

                    return next;
                }
            }
        }

        return NULL;
    }

    bool InitialiseDecoder(AVCodecContext *Context, AVPixelFormat Format)
    {
        // support and setting should have been checked in SelectAVCodec
        if (!Context || (Context && !Context->codec))
            return false;

        if (!(Format == AV_PIX_FMT_VDPAU_H264  || Format == AV_PIX_FMT_VDPAU_MPEG1 || Format == AV_PIX_FMT_VDPAU_MPEG2 ||
              Format == AV_PIX_FMT_VDPAU_MPEG4 || Format == AV_PIX_FMT_VDPAU_VC1   || Format == AV_PIX_FMT_VDPAU_WMV3) ||
            !(Context->codec->capabilities & CODEC_CAP_HWACCEL_VDPAU))
        {
            return false;
        }

        VideoVDPAU* vdpau = VideoVDPAU::Create(Context);

        if (vdpau)
        {
            Context->hwaccel_context = vdpau;
            Context->draw_horiz_band = Decode;
            Context->flags = SLICE_FLAG_CODED_ORDER | SLICE_FLAG_ALLOW_FIELD;
            return true;
        }

        return false;
    }

    void DeinitialiseDecoder(AVCodecContext *Context)
    {
        if (Context && Context->hwaccel_context && Context->codec && Context->codec->capabilities & CODEC_CAP_HWACCEL_VDPAU)
        {
            VideoVDPAU* vdpau = (VideoVDPAU*)Context->hwaccel_context;
            if (vdpau)
            {
                vdpau->SetDeleting();
                vdpau->Dereference();
                Context->hwaccel_context = NULL;
            }
        }
    }

    bool InitialiseBuffer(AVCodecContext *Context, AVFrame *Avframe, VideoFrame *Frame)
    {
        if (!Context || !Avframe || !Frame || (Context && !Context->codec))
            return false;

        if (!(Avframe->format == AV_PIX_FMT_VDPAU_H264  || Avframe->format == AV_PIX_FMT_VDPAU_MPEG1 ||
              Avframe->format == AV_PIX_FMT_VDPAU_MPEG2 || Avframe->format == AV_PIX_FMT_VDPAU_MPEG4 ||
              Avframe->format == AV_PIX_FMT_VDPAU_VC1   || Avframe->format == AV_PIX_FMT_VDPAU_WMV3) ||
            !(Context->codec->capabilities & CODEC_CAP_HWACCEL_VDPAU) || !Context->hwaccel_context)
        {
            return false;
        }

        VideoVDPAU* vdpau = (VideoVDPAU*)Context->hwaccel_context;
        if (!vdpau)
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to retrieve VideoVDPAU object from context");
            return false;
        }

        struct vdpau_render_state *render = (struct vdpau_render_state *)Frame->m_acceleratedBuffer;
        if (!render)
        {
            render = vdpau->GetSurface();

            if (!render)
            {
                LOG(VB_GENERAL, LOG_ERR, "Failed to retrieve VDPAU surface");
                return false;
            }

            Frame->m_acceleratedBuffer = render;
            Frame->m_priv[0] = (unsigned char*)vdpau;
        }

        render->state |= FF_VDPAU_STATE_USED_FOR_REFERENCE;

        Avframe->data[0]     = (uint8_t*)render;
        Avframe->data[1]     = NULL;
        Avframe->data[2]     = NULL;
        Avframe->data[3]     = NULL;
        Avframe->linesize[0] = 0;
        Avframe->linesize[1] = 0;
        Avframe->linesize[2] = 0;
        Avframe->linesize[3] = 0;

        return true;
    }

    void DeinitialiseBuffer(AVCodecContext *Context, AVFrame *Avframe, VideoFrame *Frame)
    {
        (void)Frame;

        if (!Context || !Avframe)
            return;

        if (Context->codec && Context->codec->capabilities & CODEC_CAP_HWACCEL_VDPAU && Avframe->data[0])
        {
            struct vdpau_render_state *render = (struct vdpau_render_state *)Avframe->data[0];
            if (render)
                render->state &= ~FF_VDPAU_STATE_USED_FOR_REFERENCE;
        }
    }

    void ConvertBuffer(AVFrame &Avframe, VideoFrame *Frame, SwsContext *&ConversionContext)
    {
        (void)Avframe;
        (void)Frame;
        (void)ConversionContext;
    }

    bool UpdateFrame(VideoFrame *Frame, VideoColourSpace *ColourSpace, void *Surface)
    {
        if (Frame && Surface && ColourSpace && Frame->m_acceleratedBuffer &&
            (Frame->m_pixelFormat == AV_PIX_FMT_VDPAU_H264  || Frame->m_pixelFormat == AV_PIX_FMT_VDPAU_MPEG1 ||
             Frame->m_pixelFormat == AV_PIX_FMT_VDPAU_MPEG2 || Frame->m_pixelFormat == AV_PIX_FMT_VDPAU_MPEG4 ||
             Frame->m_pixelFormat == AV_PIX_FMT_VDPAU_VC1   || Frame->m_pixelFormat == AV_PIX_FMT_VDPAU_WMV3))
        {
            //struct vdpau_render_state *render = (struct vdpau_render_state *)Frame->m_acceleratedBuffer;
            //GLTexture                *texture = static_cast<GLTexture*>(Surface);
            VideoVDPAU                 *vdpau = (VideoVDPAU*)Frame->m_priv[0];

            // add rendering here

            if (vdpau)
            {
                if (vdpau->IsDeletingOrErrored())
                {
                    Frame->m_priv[0] = NULL;
                    Frame->m_acceleratedBuffer = NULL;
                    vdpau->Dereference();
                }
            }

            return true;
        }

        return false;
    }

    bool ReleaseFrame(VideoFrame *Frame)
    {
        if (!Frame)
            return false;

        if (!(Frame->m_pixelFormat == AV_PIX_FMT_VDPAU_H264  || Frame->m_pixelFormat == AV_PIX_FMT_VDPAU_MPEG1 ||
              Frame->m_pixelFormat == AV_PIX_FMT_VDPAU_MPEG2 || Frame->m_pixelFormat == AV_PIX_FMT_VDPAU_MPEG4 ||
              Frame->m_pixelFormat == AV_PIX_FMT_VDPAU_VC1   || Frame->m_pixelFormat == AV_PIX_FMT_VDPAU_WMV3))
        {
            return false;
        }

        VideoVDPAU *vdpau = (VideoVDPAU*)Frame->m_priv[0];
        if (vdpau)
        {
            Frame->m_priv[0] = NULL;
            Frame->m_acceleratedBuffer = NULL;
            vdpau->Dereference();
            return true;
        }

        return false;
    }

    bool NeedsCustomSurfaceFormat(VideoFrame *Frame, void *Format)
    {
        return false;
    }

    bool SupportedProperties(VideoFrame *Frame, QSet<TorcPlayer::PlayerProperty> &Properties)
    {
        return false;
    }
} VDPAUFactory;
