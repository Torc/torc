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
#include "videocolourspace.h"
#include "videovdpau.h"

#include <GL/glx.h>

extern "C" {
#include "libavcodec/vdpau.h"
}

static QMutex* gVDPAULock = new QMutex(QMutex::Recursive);

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
 * \todo Fallback when no interop available (texture from pixmap)
 * \todo Colourspace
 * \todo Video chroma format checks
 * \todo Decoder profile level checks
*/

VideoVDPAU::VideoVDPAU(AVCodecContext *Context)
  : TorcReferenceCounter(),
    m_state(Created),
    m_vdpauContext(NULL),
    m_callbackLock(new QMutex()),
    m_avContext(Context),
    m_nvidiaInterop(NULL),
    m_display(NULL),
    m_createdDisplay(false),
    m_device(0),
    m_featureSet(Set_Unknown),
    m_decoder(0),
    m_lastTexture(NULL),
    m_outputSurface(0),
    m_videoMixer(0),
    m_getErrorString(&GetErrorString)
{
    m_vdpauContext = new AVVDPAUContext();
    memset((void*)m_vdpauContext, 0, sizeof(AVVDPAUContext));

    ResetProcs();
}

VideoVDPAU::~VideoVDPAU()
{
    m_state = Deleted;

    TearDown();

    delete m_callbackLock;
    delete m_vdpauContext;

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

struct vdpau_render_state* VideoVDPAU::GetSurface(void)
{
    PREEMPTION_CHECK

    if (m_createdVideoSurfaces.isEmpty() || m_state == Errored)
        return NULL;

    struct vdpau_render_state* vdpau = m_createdVideoSurfaces.takeFirst();
    m_allocatedVideoSurfaces.push_front(vdpau);
    return vdpau;
}

AVVDPAUContext* VideoVDPAU::GetContext(void)
{
    return m_vdpauContext;
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

bool VideoVDPAU::RenderFrame(VideoFrame *Frame, vdpau_render_state *Render, GLTexture *Texture, VideoColourSpace *ColourSpace)
{
    PREEMPTION_CHECK

    if (m_state == Errored || !Frame || !Render || !Texture || !ColourSpace || !m_device || !m_avContext)
        return false;

    if (!m_nvidiaInterop)
    {
        m_nvidiaInterop = new NVInterop;
        if (m_nvidiaInterop->IsValid())
            LOG(VB_GENERAL, LOG_INFO, "GL_NV_vdpau_interop available");
    }

    bool interop = m_nvidiaInterop && m_nvidiaInterop->IsValid();

    // initialise interop if present
    if (interop)
    {
        if (!m_nvidiaInterop->m_initialised)
        {
            m_nvidiaInterop->m_initialised = true;
            m_nvidiaInterop->m_init((void*)m_device, (void*)m_getProcAddress);
        }
    }

    // create an output surface if needed
    if (Texture && (m_lastTexture != Texture))
    {
        // delete the old
        if (m_outputSurface && m_outputSurfaceDestroy)
        {
            // unregister from interop
            if (interop && m_nvidiaInterop->m_registeredSurface)
            {
                m_nvidiaInterop->m_unregisterSurface(m_nvidiaInterop->m_registeredSurface);
                m_nvidiaInterop->m_registeredSurface = 0;
            }

            VdpStatus status = m_outputSurfaceDestroy(m_outputSurface);
            if (status != VDP_STATUS_OK)
                VDPAU_ERROR(status, "Failed to delete output surface");
        }

        m_outputSurface = 0;
        m_lastTexture = Texture;

        if (m_outputSurfaceCreate)
        {
            VdpStatus status = m_outputSurfaceCreate(m_device, VDP_RGBA_FORMAT_B8G8R8A8, m_avContext->width, m_avContext->height, &m_outputSurface);
            if (status != VDP_STATUS_OK)
            {
                VDPAU_ERROR(status, "Failed to create output surface");
            }
            else if (interop)
            {
                // register interop surface
                m_nvidiaInterop->m_registeredSurface = m_nvidiaInterop->m_registerOutputSurface((void*)m_outputSurface,
                                                                                                m_lastTexture->m_type,
                                                                                                1, &m_lastTexture->m_val);
                m_nvidiaInterop->m_surfaceAccess(m_nvidiaInterop->m_registeredSurface, GL_READ_ONLY);
            }
        }
    }

    // create a video mixer if needed
    if (!m_videoMixer && m_videoMixerCreate)
    {
        VdpVideoMixerParameter parameters[] = { VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_WIDTH, VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_HEIGHT };

        void const * values[] = { &m_avContext->width, &m_avContext->height };

        VdpStatus status = m_videoMixerCreate(m_device, 0, NULL, 2, parameters, values, &m_videoMixer);
        if (status != VDP_STATUS_OK)
            VDPAU_ERROR(status, "Failed to create video mixer");
    }

    // and render
    if (m_outputSurface && m_videoMixer && m_videoMixerRender)
    {
        // colourspace adjustments
        if (m_supportedMixerAttributes.contains(VDP_VIDEO_MIXER_ATTRIBUTE_CSC_MATRIX) && ColourSpace->HasChanged())
        {
            void* matrix = ColourSpace->Data();
            VdpVideoMixerAttribute attribute = { VDP_VIDEO_MIXER_ATTRIBUTE_CSC_MATRIX };
            void const *value = { matrix };
            VdpStatus status = m_videoMixerSetAttributeValues(m_videoMixer, 1, &attribute, &value);
            if (status != VDP_STATUS_OK)
                VDPAU_ERROR(status, "Failed to update video colourspace");

            ColourSpace->SetChanged(false);
        }

        // field selection
        VdpVideoMixerPictureStructure field = VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME;
        //if (Frame->m_field != VideoFrame::Frame)
        //    field = Frame->m_field == VideoFrame::TopField ? VDP_VIDEO_MIXER_PICTURE_STRUCTURE_TOP_FIELD : VDP_VIDEO_MIXER_PICTURE_STRUCTURE_BOTTOM_FIELD;

        VdpStatus status = m_videoMixerRender(m_videoMixer, VDP_INVALID_HANDLE, NULL, field, 0, NULL,
                                              Render->surface, 0, NULL, NULL, m_outputSurface, NULL, NULL, 0, NULL);
        if (status != VDP_STATUS_OK)
            VDPAU_ERROR(status, "Video mixing error");
    }

    return true;
}

bool VideoVDPAU::MapFrame(void* Surface)
{
    GLTexture *texture = static_cast<GLTexture*>(Surface);
    if (texture && (texture == m_lastTexture) && m_nvidiaInterop && m_nvidiaInterop->IsValid() &&
        m_nvidiaInterop->m_initialised && m_nvidiaInterop->m_registeredSurface)
    {
        m_nvidiaInterop->m_mapSurface(1, &m_nvidiaInterop->m_registeredSurface);
    }

    return true;
}

bool VideoVDPAU::UnmapFrame(void* Surface)
{
    GLTexture *texture = static_cast<GLTexture*>(Surface);
    if (texture && (texture == m_lastTexture) && m_nvidiaInterop && m_nvidiaInterop->IsValid() &&
        m_nvidiaInterop->m_initialised && m_nvidiaInterop->m_registeredSurface)
    {
        m_nvidiaInterop->m_unmapSurface(1, &m_nvidiaInterop->m_registeredSurface);
    }

    return true;
}

QSet<TorcPlayer::PlayerProperty> VideoVDPAU::GetSupportedProperties(void)
{
    return m_supportedProperties;
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

    m_vdpauContext->decoder = m_decoder;

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

    LOG(VB_GENERAL, LOG_INFO, QString("%1 %2 VDPAU video surfaces").arg(m_preempted.fetchAndAddOrdered(0) > 0 ? "Recreated" : "Created").arg(required));

    // check video mixer support
    m_supportedMixerAttributes.clear();
    m_supportedProperties.clear();

    if (m_videoMixerQueryAttributeSupport)
    {
        VdpBool supported = false;
        VdpStatus status = m_videoMixerQueryAttributeSupport(m_device, VDP_VIDEO_MIXER_ATTRIBUTE_CSC_MATRIX, &supported);
        if (status == VDP_STATUS_OK && supported)
        {
            m_supportedMixerAttributes << VDP_VIDEO_MIXER_ATTRIBUTE_CSC_MATRIX;
            m_supportedProperties << TorcPlayer::Brightness;
            m_supportedProperties << TorcPlayer::Contrast;
            m_supportedProperties << TorcPlayer::Saturation;
            m_supportedProperties << TorcPlayer::Hue;
        }
    }

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
    GET_PROC(VDP_FUNC_ID_OUTPUT_SURFACE_CREATE,        m_outputSurfaceCreate);
    GET_PROC(VDP_FUNC_ID_OUTPUT_SURFACE_DESTROY,       m_outputSurfaceDestroy);
    GET_PROC(VDP_FUNC_ID_VIDEO_MIXER_CREATE,           m_videoMixerCreate);
    GET_PROC(VDP_FUNC_ID_VIDEO_MIXER_DESTROY,          m_videoMixerDestroy);
    GET_PROC(VDP_FUNC_ID_VIDEO_MIXER_QUERY_ATTRIBUTE_SUPPORT, m_videoMixerQueryAttributeSupport);
    GET_PROC(VDP_FUNC_ID_VIDEO_MIXER_SET_ATTRIBUTE_VALUES, m_videoMixerSetAttributeValues);
    GET_PROC(VDP_FUNC_ID_VIDEO_MIXER_RENDER,           m_videoMixerRender);

    if (!valid)
        LOG(VB_GENERAL, LOG_ERR, "Failed to link to VDPAU library");

    // not critical
    m_getProcAddress(m_device, VDP_FUNC_ID_GET_API_VERSION, (void **)&m_getApiVersion);
    m_getProcAddress(m_device, VDP_FUNC_ID_GET_INFORMATION_STRING, (void **)&m_getInformationString);

    m_vdpauContext->render = m_decoderRender;

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

    // delete output surface
    if (m_outputSurface && m_outputSurfaceDestroy)
    {
        VdpStatus status = m_outputSurfaceDestroy(m_outputSurface);
        if (status != VDP_STATUS_OK)
            VDPAU_ERROR(status, "Failed to delete output surface");
    }
    m_outputSurface = 0;
    m_lastTexture   = NULL;

    // delete video mixer
    if (m_videoMixer && m_videoMixerDestroy)
    {
        VdpStatus status = m_videoMixerDestroy(m_videoMixer);
        if (status != VDP_STATUS_OK)
            VDPAU_ERROR(status, "Failed to delete video mixer");
    }
    m_videoMixer = 0;

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
    m_outputSurfaceCreate        = NULL;
    m_outputSurfaceDestroy       = NULL;
    m_videoMixerCreate           = NULL;
    m_videoMixerDestroy          = NULL;
    m_videoMixerQueryAttributeSupport = NULL;
    m_videoMixerSetAttributeValues    = NULL;
    m_videoMixerRender           = NULL;
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

    // FIXME - this may break if there is more than one instance
    if (m_nvidiaInterop && m_nvidiaInterop->IsValid())
    {
        if (m_nvidiaInterop->m_initialised)
            m_nvidiaInterop->m_fini();
        m_nvidiaInterop->m_initialised = false;
    }

    ResetProcs();
    m_device = 0;
    m_decoder = 0;
    m_vdpauContext->decoder = 0;
    m_lastTexture = NULL;
    m_outputSurface = 0;
    m_videoMixer = 0;


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

QMap<AVCodecContext*,VideoVDPAU*> VideoVDPAU::gVDPAUInstances;

class VDPAUFactory : public AccelerationFactory
{
    bool InitialiseDecoder(AVCodecContext *Context, AVPixelFormat Format)
    {
        if (!(Context && VideoPlayer::gAllowGPUAcceleration && VideoPlayer::gAllowGPUAcceleration->IsActive() &&
              VideoPlayer::gAllowGPUAcceleration->GetValue().toBool() && VideoVDPAU::VDPAUAvailable()))
        {
            return false;
        }

        {
            QMutexLocker locker(gVDPAULock);
            if (VideoVDPAU::gVDPAUInstances.size() > 2)
                LOG(VB_GENERAL, LOG_WARNING, QString("%1 current VideoVDPAU instances").arg(VideoVDPAU::gVDPAUInstances.size()));
        }

        if (!Context || (Context && !Context->codec) || Format != AV_PIX_FMT_VDPAU)
            return false;

        VideoVDPAU* vdpau = VideoVDPAU::Create(Context);

        if (vdpau)
        {
            {
                gVDPAULock->lock();
                VideoVDPAU::gVDPAUInstances.insert(Context, vdpau);
                gVDPAULock->unlock();
            }

            Context->pix_fmt         = AV_PIX_FMT_VDPAU;
            Context->hwaccel_context = vdpau->GetContext();
            Context->slice_flags     = SLICE_FLAG_CODED_ORDER | SLICE_FLAG_ALLOW_FIELD;
            return true;
        }

        return false;
    }

    void DeinitialiseDecoder(AVCodecContext *Context)
    {
        if (Context && Context->hwaccel_context)
        {
            VideoVDPAU* vdpau = NULL;

            {
                gVDPAULock->lock();
                if (VideoVDPAU::gVDPAUInstances.contains(Context))
                    vdpau = VideoVDPAU::gVDPAUInstances.take(Context);
                gVDPAULock->unlock();
            }

            if (vdpau)
            {
                vdpau->SetDeleting();
                vdpau->Dereference();
                Context->hwaccel_context = NULL;
                return;
            }

            LOG(VB_GENERAL, LOG_INFO, "Unknown VDPAU instance");
        }
    }

    bool InitialiseBuffer(AVCodecContext *Context, AVFrame *Avframe, VideoFrame *Frame)
    {
        if (!Context || !Avframe || !Frame)
            return false;

        if (Avframe->format != AV_PIX_FMT_VDPAU || !Context->hwaccel_context)
            return false;

        VideoVDPAU* vdpau = NULL;

        {
            gVDPAULock->lock();
            if (VideoVDPAU::gVDPAUInstances.contains(Context))
                vdpau = VideoVDPAU::gVDPAUInstances.value(Context);
            gVDPAULock->unlock();
        }

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
            Frame->m_priv[0]           = (unsigned char*)vdpau;
        }

        render->state |= FF_VDPAU_STATE_USED_FOR_REFERENCE;

        Avframe->data[0]     = (uint8_t*)render;
        Avframe->data[1]     = NULL;
        Avframe->data[2]     = NULL;
        Avframe->data[3]     = (uint8_t*)render->surface;
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

        if (Context->codec && Avframe->data[0])
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
        if (Frame && Surface && ColourSpace && Frame->m_acceleratedBuffer && Frame->m_pixelFormat == AV_PIX_FMT_VDPAU)
        {
            struct vdpau_render_state *render = (struct vdpau_render_state *)Frame->m_acceleratedBuffer;
            GLTexture                *texture = static_cast<GLTexture*>(Surface);
            VideoVDPAU                 *vdpau = (VideoVDPAU*)Frame->m_priv[0];

            if (render && vdpau && texture)
            {
                vdpau->RenderFrame(Frame, render, texture, ColourSpace);

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

        if (!Frame->m_pixelFormat == AV_PIX_FMT_VDPAU)
            return false;

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

    bool MapFrame(VideoFrame *Frame, void *Surface)
    {
        if (!Frame)
            return false;

        if (!Frame->m_pixelFormat == AV_PIX_FMT_VDPAU)
            return false;

        VideoVDPAU *vdpau = (VideoVDPAU*)Frame->m_priv[0];
        if (vdpau)
            vdpau->MapFrame(Surface);

        return true;
    }

    bool UnmapFrame(VideoFrame *Frame, void *Surface)
    {
        if (!Frame)
            return false;

        if (!Frame->m_pixelFormat == AV_PIX_FMT_VDPAU)
            return false;

        VideoVDPAU *vdpau = (VideoVDPAU*)Frame->m_priv[0];
        if (vdpau)
            vdpau->UnmapFrame(Surface);

        return true;

    }

    bool NeedsCustomSurfaceFormat(VideoFrame *Frame, void *Format)
    {
        return false;
    }

    bool SupportedProperties(VideoFrame *Frame, QSet<TorcPlayer::PlayerProperty> &Properties)
    {
        if (Frame && Frame->m_pixelFormat == AV_PIX_FMT_VDPAU && Frame->m_acceleratedBuffer && VideoVDPAU::VDPAUAvailable())
        {
            VideoVDPAU *vdpau = (VideoVDPAU*)Frame->m_priv[0];
            if (vdpau)
            {
                Properties.unite(vdpau->GetSupportedProperties());
                return true;
            }
        }

        return false;
    }
} VDPAUFactory;
