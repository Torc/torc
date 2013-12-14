/* Class VideoDecoderOMX
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2013
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

// Torc
#include "torclocalcontext.h"
#include "torclogging.h"
#include "videoplayer.h"
#include "audiodecoder.h"
#include "videodecoder.h"
#include "torcavutils.h"
#include "torcqmleventproxy.h"
#include "videodecoderomx.h"

#include "IL/OMX_Broadcom.h"

#define OPENMAX_VIDEOBUFFERS 120

VideoDecoderOMX::VideoDecoderOMX(const QString &URI, TorcPlayer *Parent, int Flags)
  : AudioDecoder(URI, Parent, Flags),
    m_core(new TorcOMXCore("openmaxil")),
    m_decoder(NULL),
    m_scheduler(NULL),
    m_render(NULL),
    m_clock(NULL),
    m_decoderToScheduler(NULL),
    m_schedulerToRender(NULL),
    m_clockToScheduler(NULL),
    m_videoParent(NULL),
    m_currentPixelFormat(PIX_FMT_NONE),
    m_currentVideoWidth(0),
    m_currentVideoHeight(0),
    m_currentReferenceCount(0)
{
    if (!m_core->IsValid())
        LOG(VB_GENERAL, LOG_ERR, "Fatal error, OpenMax is not loaded");

    VideoPlayer *player = static_cast<VideoPlayer*>(Parent);
    if (player)
        m_videoParent = player;
    else
        LOG(VB_GENERAL, LOG_ERR, "VideoDecoder does not have VideoPlayer parent");
}

VideoDecoderOMX::~VideoDecoderOMX()
{
}

bool VideoDecoderOMX::VideoBufferStatus(int &Unused, int &Inuse, int &Held)
{
    Held   = 0;
    Unused = 1; // if errored, avoid a stall
    Inuse  = 0; // and let the decoder exit on completion

    if (m_decoder && m_decoder->IsValid())
    {
        Unused = m_decoder->GetAvailableBuffers(OMX_DirInput, 0, OMX_IndexParamVideoInit);
        Inuse  = OPENMAX_VIDEOBUFFERS - Unused;
    }

    return true;
}

void VideoDecoderOMX::ProcessVideoPacket(AVFormatContext *Context, AVStream *Stream, AVPacket *Packet)
{
    if (!Context || !Packet || !Stream || (Stream && !Stream->codec) || !m_decoder)
        return;

    AVFrame avframe;
    memset(&avframe, 0, sizeof(AVFrame));
    avcodec_get_frame_defaults(&avframe);
    int gotframe = 0;

    int libavused = 0;//avcodec_decode_video2(Stream->codec, &avframe, &gotframe, Packet);

    OMX_U8* data = Packet->data;
    int bytestoconsume = Packet->size;

    qint64 pts = av_q2d(Stream->time_base) * 1000000 * (Packet->pts - Stream->start_time);

    while (bytestoconsume > 0)
    {
        OMX_BUFFERHEADERTYPE *buffer = m_decoder->GetInputBuffer(0, 500, OMX_IndexParamVideoInit);
        if (!buffer)
        {
           LOG(VB_GENERAL, LOG_ERR, "Timed out waiting for free video decoder buffer");
           break;
        }

        OMX_TICKS ticks;
        ticks.nLowPart  = pts;
        ticks.nHighPart = pts >> 32;

        int size = std::min(bytestoconsume, (int)buffer->nAllocLen);
        memcpy(buffer->pBuffer, data, size);
        bytestoconsume    -= size;
        data              += size;
        buffer->nFlags     = bytestoconsume == 0 ? OMX_BUFFERFLAG_ENDOFFRAME : 0;
        buffer->nOffset    = 0;
        buffer->nTimeStamp = ticks;
        buffer->nFilledLen = size;

        static int count  = 0;
        static bool first = true;
        if (first)
            buffer->nFlags |= OMX_BUFFERFLAG_STARTTIME;
        first = false;

        //LOG(VB_GENERAL, LOG_INFO, QString("%1: pts: %2: frame: %3 size: %4 libavused: %5").arg(count++).arg(pts).arg(gotframe).arg(Packet->size).arg(libavused));

        m_decoder->EmptyThisBuffer(buffer);
    }
}

void VideoDecoderOMX::SetupVideoDecoder(AVFormatContext *Context, AVStream *Stream)
{
    if (!Stream || (Stream && !Stream->codec))
        return;

    // setup libav parsing
    AVCodecContext *context        = Stream->codec;
    context->thread_count          = 1;
    context->thread_safe_callbacks = 1;
    context->thread_type           = FF_THREAD_SLICE;
    context->workaround_bugs       = FF_BUG_AUTODETECT;
    context->opaque                = (void*)this;
    context->skip_frame            = AVDISCARD_ALL;

    // track format internally
    SetFormat(context->pix_fmt, context->width, context->height, context->refs, false);

    // TODO fallback to software decode
    // bailout early if OpenMax is not availble
    if (!m_core->IsValid())
    {
        LOG(VB_GENERAL, LOG_ERR, "OpenMax decoding not available");
        return;
    }

    // decide OpenMax codec type
    OMX_VIDEO_CODINGTYPE omxcodec = OMX_VIDEO_CodingAutoDetect;
    switch (Stream->codec->codec_id)
    {
        case AV_CODEC_ID_H264:       omxcodec = OMX_VIDEO_CodingAVC; break;
        case AV_CODEC_ID_H263:
        case AV_CODEC_ID_MPEG4:      omxcodec = OMX_VIDEO_CodingMPEG4; break;
        case AV_CODEC_ID_MPEG1VIDEO:
        case AV_CODEC_ID_MPEG2VIDEO: omxcodec = OMX_VIDEO_CodingMPEG2; break;
        case AV_CODEC_ID_VP6:
        case AV_CODEC_ID_VP6A:
        case AV_CODEC_ID_VP6F:       omxcodec = OMX_VIDEO_CodingVP6; break;
        case AV_CODEC_ID_VP8:        omxcodec = OMX_VIDEO_CodingVP8; break;
        case AV_CODEC_ID_THEORA:     omxcodec = OMX_VIDEO_CodingTheora; break;
        case AV_CODEC_ID_MJPEG:
        case AV_CODEC_ID_MJPEGB:     omxcodec = OMX_VIDEO_CodingMJPEG; break;
        case AV_CODEC_ID_VC1:
        case AV_CODEC_ID_WMV3:       omxcodec = OMX_VIDEO_CodingWMV; break;
        default: break;
    }

    // create components
    m_decoder   = new TorcOMXComponent(m_core, (char*)"OMX.broadcom.video_decode");
    m_scheduler = new TorcOMXComponent(m_core, (char*)"OMX.broadcom.video_scheduler");
    m_render    = new TorcOMXComponent(m_core, (char*)"OMX.broadcom.video_render");
    m_clock     = new TorcOMXComponent(m_core, (char*)"OMX.broadcom.clock");

    OMX_U32 decodeport = m_decoder->GetPort(OMX_DirInput, 0, OMX_IndexParamVideoInit);

    // check video codec support (move this)
    OMX_VIDEO_PARAM_PORTFORMATTYPE portformat;
    OMX_INITSTRUCTURE(portformat);
    OMX_U32 index = 0;
    portformat.nPortIndex = decodeport;
    portformat.nIndex = index;

    while (m_decoder->GetParameter(OMX_IndexParamVideoPortFormat, &portformat) == OMX_ErrorNone)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("%1: Supported codec %2").arg(m_decoder->GetName()).arg(portformat.eCompressionFormat, 0, 16));
        index++;
        OMX_INITSTRUCTURE(portformat);
        portformat.nPortIndex = decodeport;
        portformat.nIndex = index;
    }

    // configure decoder
    OMX_INITSTRUCTURE(portformat);
    portformat.nPortIndex         = decodeport;
    portformat.eCompressionFormat = omxcodec;
    portformat.xFramerate         = VideoDecoder::GetFrameRate(Context, Stream) * (1<<16);

    LOG(VB_GENERAL, LOG_INFO, QString("Setting scheduler framerate to %1").arg(VideoDecoder::GetFrameRate(Context, Stream)));

    if (m_decoder->SetParameter(OMX_IndexParamVideoPortFormat, &portformat) != OMX_ErrorNone)
       LOG(VB_GENERAL, LOG_ERR, "Failed to set port format");

    OMX_PARAM_PORTDEFINITIONTYPE portdefinition;
    OMX_INITSTRUCTURE(portdefinition);
    portdefinition.nPortIndex = decodeport;

    if (m_decoder->GetParameter(OMX_IndexParamPortDefinition, &portdefinition) == OMX_ErrorNone)
    {
       portdefinition.nPortIndex                = decodeport;
       portdefinition.nBufferCountActual        = OPENMAX_VIDEOBUFFERS;
       portdefinition.format.video.nFrameWidth  = m_currentVideoWidth;
       portdefinition.format.video.nFrameHeight = m_currentVideoHeight;

       if (m_decoder->SetParameter(OMX_IndexParamPortDefinition, &portdefinition) != OMX_ErrorNone)
           LOG(VB_GENERAL, LOG_ERR, "Failed to set port definition");
    }
    else
    {
       LOG(VB_GENERAL, LOG_ERR, "Failed to get port definition");
    }

    // check h264
    if (OMX_VIDEO_CodingAVC == omxcodec)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("h264 extradata size: %1").arg(context->extradata_size));
        if ((context->extradata_size < 7) || !context->extradata || ((context->extradata_size > 0) && context->extradata && (*context->extradata == 1)))
        {
            LOG(VB_GENERAL, LOG_INFO, "Setting NaluFormatStartCodes");
            OMX_NALSTREAMFORMATTYPE nal;
            OMX_INITSTRUCTURE(nal);
            nal.nPortIndex  = decodeport;
            nal.eNaluFormat = OMX_NaluFormatStartCodes;
            m_decoder->SetParameter((OMX_INDEXTYPE)OMX_IndexParamNalStreamFormatSelect, &nal);
        }
    }

    // configure clock
    OMX_TIME_CONFIG_CLOCKSTATETYPE clockstate;
    OMX_INITSTRUCTURE(clockstate);
    clockstate.eState = OMX_TIME_ClockStateWaitingForStartTime;
    clockstate.nWaitMask = 1;
    m_clock->SetParameter(OMX_IndexConfigTimeClockState, &clockstate);

    // configure render
    OMX_S32 layer = 0;
    TorcQMLEventProxy *proxy = static_cast<TorcQMLEventProxy*>(gLocalContext->GetUIObject());
    if (proxy && proxy->GetWindow())
    {
        layer = proxy->GetWindow()->winId();
        LOG(VB_GENERAL, LOG_INFO, QString("Window (layer) id: %1").arg(layer));
    }

    if (!layer)
        LOG(VB_GENERAL, LOG_ERR, "Failed to get window layer - video will be hidden");

    OMX_CONFIG_DISPLAYREGIONTYPE display;
    OMX_INITSTRUCTURE(display);
    display.nPortIndex = m_render->GetPort(OMX_DirInput, 0, OMX_IndexParamVideoInit);
    display.set        = OMX_DISPLAY_SET_LAYER;
    display.layer      = layer;
    m_render->SetConfig(OMX_IndexConfigDisplayRegion, &display);

    // create tunnels
    m_decoderToScheduler = new TorcOMXTunnel(m_core, m_decoder, 0, OMX_IndexParamVideoInit, m_scheduler, 0, OMX_IndexParamVideoInit);
    m_schedulerToRender  = new TorcOMXTunnel(m_core, m_scheduler, 0, OMX_IndexParamVideoInit, m_render, 0, OMX_IndexParamVideoInit);
    m_clockToScheduler   = new TorcOMXTunnel(m_core, m_clock, 0, OMX_IndexParamOtherInit, m_scheduler, 0, OMX_IndexParamOtherInit);

    m_decoderToScheduler->Create();
    m_schedulerToRender->Create();
    m_clockToScheduler->Create();

    // enable ports and transition to idle
    m_render->EnablePort(OMX_DirInput, 0, true, OMX_IndexParamVideoInit);
    m_scheduler->EnablePort(OMX_DirOutput, 0, true, OMX_IndexParamVideoInit);
    m_scheduler->EnablePort(OMX_DirInput, 0, true, OMX_IndexParamVideoInit);
    m_scheduler->EnablePort(OMX_DirInput, 0, true, OMX_IndexParamOtherInit);
    m_clock->EnablePort(OMX_DirOutput, 0, true, OMX_IndexParamOtherInit);
    m_decoder->EnablePort(OMX_DirOutput, 0, true, OMX_IndexParamVideoInit);

    m_clock->SetState(OMX_StateIdle, false);
    m_render->SetState(OMX_StateIdle);
    m_scheduler->SetState(OMX_StateIdle);

    // create decoder buffers and transition decoder to idle
    m_decoder->EnablePort(OMX_DirInput, 0, true, OMX_IndexParamVideoInit);
    m_decoder->SetState(OMX_StateIdle, false);
    m_decoder->CreateBuffers(OMX_DirInput, 0, OMX_IndexParamVideoInit);

    m_decoder->WaitForResponse(OMX_CommandStateSet, OMX_StateIdle, 1000);
    m_clock->WaitForResponse(OMX_CommandStateSet, OMX_StateIdle, 1000);

    // start
    m_render->SetState(OMX_StateExecuting);
    m_scheduler->SetState(OMX_StateExecuting);
    m_clock->SetState(OMX_StateExecuting);
    m_decoder->SetState(OMX_StateExecuting);

    // send codec config
    OMX_BUFFERHEADERTYPE *buffer = m_decoder->GetInputBuffer(0, 1000, OMX_IndexParamVideoInit);
    if (buffer)
    {
        buffer->nOffset = 0;
        buffer->nFilledLen = context->extradata_size;
        memset((unsigned char*)buffer->pBuffer, 0, buffer->nAllocLen);
        memcpy((unsigned char*)buffer->pBuffer, context->extradata, context->extradata_size);
        buffer->nFlags = OMX_BUFFERFLAG_CODECCONFIG | OMX_BUFFERFLAG_ENDOFFRAME;

        if (m_decoder->EmptyThisBuffer(buffer) == OMX_ErrorNone)
            LOG(VB_GENERAL, LOG_INFO, "Sent decoder configuration");
        else
            LOG(VB_GENERAL, LOG_ERR, "Failed to send decoder configuration");
    }
}


void VideoDecoderOMX::CleanupVideoDecoder(AVStream *Stream)
{
}

void VideoDecoderOMX::FlushVideoBuffers(bool Stopped)
{
}

void VideoDecoderOMX::SetFormat(PixelFormat Format, int Width, int Height, int References, bool UpdateParent)
{
    m_currentPixelFormat    = Format;
    m_currentVideoWidth     = Width;
    m_currentVideoHeight    = Height;
    m_currentReferenceCount = References;
}

class VideoDecoderOMXFactory : public TorcDecoderFactory
{
    void Score(int DecodeFlags, const QString &URI, int &Score, TorcPlayer *Parent)
    {
        if ((DecodeFlags & TorcDecoder::DecodeVideo) && Score <= 60)
            Score = 60;
    }

    TorcDecoder* Create(int DecodeFlags, const QString &URI, int &Score, TorcPlayer *Parent)
    {
        if ((DecodeFlags & TorcDecoder::DecodeVideo) && Score <= 60)
            return new VideoDecoderOMX(URI, Parent, DecodeFlags);

        return NULL;
    }
} VideoDecoderOMXFactory;
