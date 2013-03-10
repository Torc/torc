/* Class VideoDecoder
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012
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
#include "torclogging.h"
#include "videoplayer.h"
#include "audiodecoder.h"
#include "torcavutils.h"
#include "videoframe.h"
#include "videodecoder.h"

#if CONFIG_VDA
#define VDA_CV_FORMAT  kCVPixelFormatType_422YpCbCr8
#define VDA_PIX_FORMAT PIX_FMT_UYVY422
#include "CoreVideo/CVPixelBuffer.h"
extern "C" {
#include "libavcodec/vda.h"
}
#endif

#define SANE_ASPECT_RATIO(Val) (Val > 0.1f && Val < 10.0f)

static PixelFormat GetFormatDefault(AVCodecContext *Context, const PixelFormat *Formats);

double VideoDecoder::GetFrameAspectRatio(AVStream *Stream, AVFrame &Frame)
{
    qreal result = 0.0f;

    if (Frame.height > 0 && Frame.sample_aspect_ratio.num)
    {
        result = av_q2d(Frame.sample_aspect_ratio) * ((double)Frame.width / (double)Frame.height);

        if (SANE_ASPECT_RATIO(result))
            return result;
    }

    if (Stream)
    {
        if (Stream->codec && Stream->codec->height > 0)
        {
            if (Stream->codec->sample_aspect_ratio.num)
            {
                result = av_q2d(Stream->codec->sample_aspect_ratio) * ((double)Stream->codec->width / (double)Stream->codec->height);

                if (SANE_ASPECT_RATIO(result))
                    return result;
            }

            result = (double)Stream->codec->width / (double)Stream->codec->height;

            if (SANE_ASPECT_RATIO(result))
                return result;
        }

        if (Stream->sample_aspect_ratio.num)

        if ((result <= 0.1f || result > 10.0f) && Stream->codec->height)
            result = (double)Stream->codec->width / (double)Stream->codec->height;
    }

    if (Frame.height > 0)
        result = (double)Frame.width / (double)Frame.height;

    if (SANE_ASPECT_RATIO(result))
        return result;

    return 4.0f / 3.0f;
}

double VideoDecoder::GetPixelAspectRatio(AVStream *Stream, AVFrame &Frame)
{
    double result = 0.0f;

    if (Frame.sample_aspect_ratio.num)
        result = av_q2d(Frame.sample_aspect_ratio);

    if (SANE_ASPECT_RATIO(result))
        return result;

    if (Stream)
    {
        if (Stream->codec && Stream->codec->sample_aspect_ratio.num)
            result = av_q2d(Stream->codec->sample_aspect_ratio);

        if (SANE_ASPECT_RATIO(result))
            return result;

        result = av_q2d(Stream->sample_aspect_ratio);

        if (SANE_ASPECT_RATIO(result))
            return result;
    }

    LOG(VB_GENERAL, LOG_WARNING, "Failed to get sensible pixel aspect ratio");
    return 1.0f;
}

double VideoDecoder::GetFrameRate(AVFormatContext *Context, AVStream *Stream)
{
    if (!Stream)
        return 30000.0f / 1001.0f;

    double rate      = 0.0f;
    double average   = 0.0f;
    double codec     = 0.0f;
    double container = 0.0f;
    double estimated = 0.0f;

    // default for matroska
    if (Stream->avg_frame_rate.den && Stream->avg_frame_rate.num)
        average = av_q2d(Stream->avg_frame_rate);

    if (Stream->codec)
    {
        if (Stream->codec->time_base.den && Stream->codec->time_base.num)
            codec = 1.0f / av_q2d(Stream->codec->time_base) / Stream->codec->ticks_per_frame;

        if (codec > 121.0f && (Stream->codec->time_base.den > 10000) && (Stream->codec->time_base.num == 1))
        {
            Stream->codec->time_base.num = 1001;
            if (av_q2d(Stream->codec->time_base) > 0)
                codec = 1.0f / av_q2d(Stream->codec->time_base);
        }
    }

    if (Stream->time_base.den && Stream->time_base.num)
        container = 1.0f / av_q2d(Stream->time_base);

    if (Stream->r_frame_rate.den && Stream->r_frame_rate.num)
        estimated = av_q2d(Stream->r_frame_rate);

    if ((QString(Context->iformat->name).contains("matroska") || QString(Context->iformat->name).contains("mov")) && average < 121.0f && average > 3.0f)
        rate = average;
    else if (QString(Context->iformat->name).contains("avi") && container < 121.0f && container > 3.0f)
        rate = container;
    else if (codec < 121.0f && codec > 3.0f)
        rate = codec;
    else if (container < 121.0f && container > 3.0f)
        rate = container;
    else if (estimated < 121.0f && estimated > 3.0f)
        rate = estimated;
    else if (average < 121.0f && average > 3.0f)
        rate = average;
    else
        rate = 30000.0f / 1001.0f;

    return rate;
}

static int GetBuffer(struct AVCodecContext *Context, AVFrame *Frame)
{
    if (!Context->codec)
        return -1;

    if (!(Context->codec->capabilities & CODEC_CAP_DR1))
        return avcodec_default_get_buffer(Context, Frame);

    VideoDecoder *parent = static_cast<VideoDecoder*>(Context->opaque);
    if (parent)
        return parent->GetAVBuffer(Context, Frame);

    LOG(VB_GENERAL, LOG_ERR, "Invalid context");
    return -1;
}

int VideoDecoder::GetAVBuffer(AVCodecContext *Context, AVFrame *Frame)
{
    VideoFrame *frame  = m_videoParent->GetBuffers()->GetFrameForDecoding();

    if (!frame)
        return -1;

    if (Context->width != frame->m_rawWidth || Context->height != frame->m_rawHeight || Context->pix_fmt != frame->m_pixelFormat)
        LOG(VB_GENERAL, LOG_ERR, "Frame format changed");

    static uint8_t dummy = 1;
    bool hardware = Context->pix_fmt == PIX_FMT_VDA_VLD;

    for (int i = 0; i < 4; i++)
    {
        Frame->data[i]     = hardware ? &dummy : frame->m_buffer + frame->m_offsets[i];
        Frame->base[i]     = hardware ? &dummy : Frame->data[i];
        Frame->linesize[i] = hardware ? dummy  : frame->m_pitches[i];
    }

    Frame->opaque              = frame;
    Frame->type                = FF_BUFFER_TYPE_USER;
    Frame->extended_data       = Frame->data;
    Frame->pkt_pts             = Context->pkt ? Context->pkt->pts : AV_NOPTS_VALUE;
    Frame->pkt_dts             = Context->pkt ? Context->pkt->dts : AV_NOPTS_VALUE;
    Frame->width               = frame->m_rawWidth;
    Frame->height              = frame->m_rawHeight;
    Frame->format              = frame->m_pixelFormat;
    Frame->sample_aspect_ratio = Context->sample_aspect_ratio;

    return 0;
}

static void ReleaseBuffer(AVCodecContext *Context, AVFrame *Frame)
{
    if (Frame->type == FF_BUFFER_TYPE_INTERNAL)
    {
        avcodec_default_release_buffer(Context, Frame);
        return;
    }

    VideoDecoder *parent = static_cast<VideoDecoder*>(Context->opaque);
    if (parent)
        parent->ReleaseAVBuffer(Context, Frame);
    else
        LOG(VB_GENERAL, LOG_ERR, "Invalid context");
}

void VideoDecoder::ReleaseAVBuffer(AVCodecContext *Context, AVFrame *Frame)
{
    m_videoParent->GetBuffers()->ReleaseFrameFromDecoded(static_cast<VideoFrame*>(Frame->opaque));

    if (Frame->type != FF_BUFFER_TYPE_USER)
        LOG(VB_GENERAL, LOG_ERR, "Unexpected buffer type");

#if CONFIG_VDA
    if (Frame->format == PIX_FMT_VDA_VLD && Context->hwaccel_context)
    {
        CVPixelBufferRef buffer = (CVPixelBufferRef)Frame->data[3];
        if (buffer)
            CFRelease(buffer);
    }
#endif

    for (uint i = 0; i < 4; i++)
        Frame->data[i] = NULL;
}

static PixelFormat GetFormat(AVCodecContext *Context, const PixelFormat *Formats)
{
    if (!Context || !Formats)
        return PIX_FMT_YUV420P;

    VideoDecoder* parent = static_cast<VideoDecoder*>(Context->opaque);
    if (parent)
        return parent->AgreePixelFormat(Context, Formats);

    return GetFormatDefault(Context, Formats);
}

PixelFormat GetFormatDefault(AVCodecContext *Context, const PixelFormat *Formats)
{
    if (!Context || !Formats)
        return PIX_FMT_YUV420P;

    // return the last format in the list, which should avoid any hardware formats
    PixelFormat *formats = const_cast<PixelFormat*>(Formats);
    PixelFormat   format = *formats;
    while (*formats != -1)
        format = *(formats++);

    return format;
}

PixelFormat VideoDecoder::AgreePixelFormat(AVCodecContext *Context, const PixelFormat *Formats)
{
    if (!Context || !Formats)
        return PIX_FMT_YUV420P;

    VideoDecoder* parent = static_cast<VideoDecoder*>(Context->opaque);
    if (parent != this)
        return GetFormatDefault(Context, Formats);

    PixelFormat *formats = const_cast<PixelFormat*>(Formats);
    PixelFormat format   = *formats;
    while (*formats != -1)
    {
        format = *(formats++);
        LOG(VB_GENERAL, LOG_INFO, QString("Testing pixel format: %1").arg(av_get_pix_fmt_name(format)));

#if CONFIG_VDA
        if (format == PIX_FMT_VDA_VLD && Context->codec_id == AV_CODEC_ID_H264 && 0)
        {
            struct vda_context *context = new vda_context;
            memset(context, 0, sizeof(vda_context));
            context->decoder           = NULL;
            context->use_sync_decoding = 1;
            context->width             = Context->width;
            context->height            = Context->height;
            context->cv_pix_fmt_type   = VDA_CV_FORMAT;
            context->format            = 'avc1';

            if (ff_vda_create_decoder(context, Context->extradata, Context->extradata_size))
            {
                LOG(VB_GENERAL, LOG_ERR, "Error creating VDA decoder");
                delete context;
            }
            else
            {
                Context->hwaccel_context = context;
                break;
            }
        }
#endif
        if (format == PIX_FMT_YUV420P)
            break;
    }

    SetFormat(format, Context->width, Context->height, Context->refs, true);
    return format;
}

VideoDecoder::VideoDecoder(const QString &URI, TorcPlayer *Parent, int Flags)
  : AudioDecoder(URI, Parent, Flags),
    m_keyframeSeen(false),
    m_videoParent(NULL),
    m_currentPixelFormat(PIX_FMT_NONE),
    m_currentVideoWidth(0),
    m_currentVideoHeight(0),
    m_currentReferenceCount(0),
    m_conversionContext(NULL)
{
    ResetPTSTracker();

    VideoPlayer *player = static_cast<VideoPlayer*>(Parent);
    if (player)
        m_videoParent = player;
    else
        LOG(VB_GENERAL, LOG_ERR, "VideoDecoder does not have VideoPlayer parent");
}

VideoDecoder::~VideoDecoder()
{
    if (m_conversionContext)
        sws_freeContext(m_conversionContext);
}

bool VideoDecoder::VideoBufferStatus(int &Unused, int &Inuse, int &Held)
{
    return m_videoParent->GetBuffers()->GetBufferStatus(Unused, Inuse, Held);
}

void VideoDecoder::ProcessVideoPacket(AVFormatContext *Context, AVStream *Stream, AVPacket *Packet)
{
    if (!Context || !Packet || !Stream || (Stream && !Stream->codec))
        return;

    // sanity check for format changes
    if (Stream->codec->pix_fmt != m_currentPixelFormat ||
        Stream->codec->width   != m_currentVideoWidth  ||
        Stream->codec->height  != m_currentVideoHeight ||
        Stream->codec->refs    != m_currentReferenceCount)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Video format changed from %1 %2x%3 (%4refs) to %5 %6x%7 %8")
            .arg(av_get_pix_fmt_name(m_currentPixelFormat)).arg(m_currentVideoWidth).arg(m_currentVideoHeight).arg(m_currentReferenceCount)
            .arg(av_get_pix_fmt_name(Stream->codec->pix_fmt)).arg(Stream->codec->width).arg(Stream->codec->height).arg(Stream->codec->refs));

        SetFormat(Stream->codec->pix_fmt, Stream->codec->width, Stream->codec->height, Stream->codec->refs, true);
    }

    // Add any preprocessing here

    // Decode a frame
    AVFrame avframe;
    memset(&avframe, 0, sizeof(AVFrame));
    avcodec_get_frame_defaults(&avframe);
    int gotframe = 0;

    int result = avcodec_decode_video2(Stream->codec, &avframe, &gotframe, Packet);

    if (result < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Unknown video decoding error (%1)").arg(result));
        return;
    }

    if (!gotframe)
        return;

    // mark frames that may be corrupt
    if (!m_keyframeSeen && avframe.key_frame)
        m_keyframeSeen = true;

    VideoFrame *frame = avframe.opaque ? static_cast<VideoFrame*>(avframe.opaque) : m_videoParent->GetBuffers()->GetFrameForDecoding();

    if (!frame)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to get video frame");
        return;
    }

#if CONFIG_VDA
    if (avframe.format == PIX_FMT_VDA_VLD && frame->m_pixelFormat == PIX_FMT_VDA_VLD)
    {
        CVPixelBufferRef buffer = (CVPixelBufferRef)avframe.data[3];
        if (buffer)
        {
            CVPixelBufferLockBaseAddress(buffer, 0);

            int width  = frame->m_rawWidth;
            int height = frame->m_rawHeight;

            AVPicture in;
            memset(&in, 0, sizeof(AVPicture));
            uint planes = std::max((uint)1, (uint)CVPixelBufferGetPlaneCount(buffer));
            for (uint i = 0; i < planes; ++i)
            {
                in.data[i]     = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(buffer, i);
                in.linesize[i] = CVPixelBufferGetBytesPerRowOfPlane(buffer, i);
            }

            AVPicture out;
            avpicture_fill(&out, frame->m_buffer, frame->m_secondaryPixelFormat, frame->m_rawWidth, frame->m_rawHeight);

            m_conversionContext = sws_getCachedContext(m_conversionContext,
                                                       width, height, VDA_PIX_FORMAT,
                                                       width, height, frame->m_secondaryPixelFormat,
                                                       SWS_FAST_BILINEAR, NULL, NULL, NULL);
            if (m_conversionContext != NULL)
            {
                if (sws_scale(m_conversionContext, in.data, in.linesize, 0, height, out.data, out.linesize) < 1)
                    LOG(VB_GENERAL, LOG_ERR, "Software scaling/conversion failed");
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, "Failed to create software conversion context");
            }

            CVPixelBufferUnlockBaseAddress(buffer, 0);
        }
    }
#endif

    frame->m_colourSpace      = Stream->codec->colorspace;
    frame->m_topFieldFirst    = avframe.top_field_first;
    frame->m_interlaced       = avframe.interlaced_frame;
    frame->m_frameAspectRatio = GetFrameAspectRatio(Stream, avframe);
    frame->m_pixelAspectRatio = GetPixelAspectRatio(Stream, avframe);
    frame->m_repeatPict       = avframe.repeat_pict;
    frame->m_frameNumber      = avframe.display_picture_number;
    frame->m_pts              = av_q2d(Stream->time_base) * 1000 * GetPTS(avframe.pkt_pts, avframe.pkt_dts);
    frame->m_corrupt          = !m_keyframeSeen;
    frame->m_frameRate        = GetFrameRate(Context, Stream);

    m_videoParent->GetBuffers()->ReleaseFrameFromDecoding(frame);
}

void VideoDecoder::SetupVideoDecoder(AVFormatContext *Context, AVStream *Stream)
{
    (void)Context;

    if (!Stream || (Stream && !Stream->codec))
        return;

    int threads = 1;

    AVCodecContext *context        = Stream->codec;
    context->thread_count          = threads;
    context->thread_safe_callbacks = 1;
    context->thread_type           = FF_THREAD_SLICE;
    context->draw_horiz_band       = NULL;
    context->slice_flags           = 0;
    context->err_recognition       = 0;
    context->workaround_bugs       = FF_BUG_AUTODETECT;
    context->error_concealment     = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
    context->idct_algo             = FF_IDCT_AUTO;
    context->debug                 = 0;
    context->error_rate            = 0;
    context->opaque                = (void*)this;
    context->get_buffer            = GetBuffer;
    context->release_buffer        = ReleaseBuffer;
    context->get_format            = GetFormat;

    AVCodec *codec = avcodec_find_decoder(context->codec_id);

    if (codec && (codec->capabilities & CODEC_CAP_DR1))
    {
        context->flags            |= CODEC_FLAG_EMU_EDGE;
    }

    SetFormat(context->pix_fmt, context->width, context->height, context->refs, false);
}

void VideoDecoder::CleanupVideoDecoder(AVStream *Stream)
{
    if (!Stream || (Stream && !Stream->codec))
        return;

    sws_freeContext(m_conversionContext);
    m_conversionContext = NULL;

#if CONFIG_VDA
    if (Stream->codec->hwaccel_context && Stream->codec->pix_fmt == PIX_FMT_VDA_VLD)
    {
        vda_context *context = static_cast<vda_context*>(Stream->codec->hwaccel_context);
        if (context)
        {
            LOG(VB_PLAYBACK, LOG_INFO, "Destroying VDA decoder");
            ff_vda_destroy_decoder(context);
            delete context;
            Stream->codec->hwaccel_context = NULL;
        }
    }
#endif
}

void VideoDecoder::FlushVideoBuffers(bool Stopped)
{
    ResetPTSTracker();

    if (Stopped)
        m_videoParent->Reset();
    else
        m_videoParent->GetBuffers()->Reset(false);
    m_keyframeSeen = false;
}

void VideoDecoder::SetFormat(PixelFormat Format, int Width, int Height, int References, bool UpdateParent)
{
    m_currentPixelFormat    = Format;
    m_currentVideoWidth     = Width;
    m_currentVideoHeight    = Height;
    m_currentReferenceCount = References;

    if (UpdateParent)
        m_videoParent->GetBuffers()->FormatChanged(m_currentPixelFormat, m_currentVideoWidth, m_currentVideoHeight, m_currentReferenceCount);
}

void VideoDecoder::ResetPTSTracker(void)
{
    m_lastPTS = INT64_MIN;
    m_lastDTS = INT64_MIN;
    m_faultyPTSCount = 0;
    m_faultyDTSCount = 0;
}

int64_t VideoDecoder::GetPTS(int64_t PTS, int64_t DTS)
{
    if (DTS != (int64_t)AV_NOPTS_VALUE)
    {
        m_faultyDTSCount += DTS <= m_lastDTS;
        m_lastDTS = DTS;
    }

    if (PTS != (int64_t)AV_NOPTS_VALUE)
    {
        m_faultyPTSCount += PTS <= m_lastPTS;
        m_lastPTS = PTS;
    }

    if ((m_faultyPTSCount <= m_faultyDTSCount || DTS == (int64_t)AV_NOPTS_VALUE) && PTS != (int64_t)AV_NOPTS_VALUE)
        return PTS;

    return DTS;
}

class VideoDecoderFactory : public DecoderFactory
{
    void Score(int DecodeFlags, const QString &URI, int &Score, TorcPlayer *Parent)
    {
        if ((DecodeFlags & TorcDecoder::DecodeVideo) && Score <= 50)
            Score = 50;
    }

    TorcDecoder* Create(int DecodeFlags, const QString &URI, int &Score, TorcPlayer *Parent)
    {
        if ((DecodeFlags & TorcDecoder::DecodeVideo) && Score <= 50)
            return new VideoDecoder(URI, Parent, DecodeFlags);

        return NULL;
    }
} VideoDecoderFactory;

