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

extern "C" {
#include "libavutil/pixfmt.h"
#include "libavutil/pixdesc.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
}

static inline double GetAspectRatio(AVStream *Stream, AVFrame &Frame)
{
    qreal result = 0.0f;

    if (Frame.height && Frame.sample_aspect_ratio.num)
        result = av_q2d(Frame.sample_aspect_ratio) * ((double)Frame.width / (double)Frame.height);

    if ((result <= 0.1f || result > 10.0f) && Stream && Stream->codec)
    {
        if (Stream->codec->height && Stream->codec->sample_aspect_ratio.num)
            result = av_q2d(Stream->codec->sample_aspect_ratio) * ((double)Stream->codec->width / (double)Stream->codec->height);

        if ((result <= 0.1f || result > 10.0f) && Frame.height)
            result = (double)Frame.width / (double)Frame.height;

        if ((result <= 0.1f || result > 10.0f) && Stream->codec->height)
            result = (double)Stream->codec->width / (double)Stream->codec->height;
    }

    if (result <= 0.1f || result > 10.0f)
        result = 4.0f / 3.0f;

    return result;
}

static int GetBuffer(struct AVCodecContext *Context, AVFrame *Frame)
{
    if (!Context->codec)
        return -1;

    if (!Context->codec->capabilities & CODEC_CAP_DR1)
        return avcodec_default_get_buffer(Context, Frame);

    VideoPlayer *parent = (VideoPlayer*)Context->opaque;
    VideoFrame  *frame  = parent ? parent->Buffers()->GetFrameForDecoding() : NULL;

    if (!frame)
        return -1;

    if (Context->width != frame->m_rawWidth || Context->height != frame->m_rawHeight || Context->pix_fmt != frame->m_pixelFormat)
        LOG(VB_GENERAL, LOG_ERR, "Frame format changed");

    for (int i = 0; i < 4; i++)
    {
        Frame->base[i]     = frame->m_buffer;
        Frame->data[i]     = frame->m_buffer + frame->m_offsets[i];
        Frame->linesize[i] = frame->m_pitches[i];
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

    VideoPlayer *parent = (VideoPlayer*)Context->opaque;
    if (parent)
        parent->Buffers()->ReleaseFrameFromDecoded((VideoFrame*)Frame->opaque);

    if (Frame->type != FF_BUFFER_TYPE_USER)
        LOG(VB_GENERAL, LOG_ERR, "Unexpected buffer type");

    for (uint i = 0; i < 4; i++)
        Frame->data[i] = NULL;
}

VideoDecoder::VideoDecoder(const QString &URI, TorcPlayer *Parent, int Flags)
  : AudioDecoder(URI, Parent, Flags),
    m_videoParent(NULL),
    m_currentPixelFormat(PIX_FMT_NONE),
    m_currentVideoWidth(0),
    m_currentVideoHeight(0),
    m_currentReferenceCount(0)
{
    VideoPlayer *player = static_cast<VideoPlayer*>(Parent);
    if (player)
        m_videoParent = player;
    else
        LOG(VB_GENERAL, LOG_ERR, "VideoDecoder does not have VideoPlayer parent");
}

VideoDecoder::~VideoDecoder()
{
}

bool VideoDecoder::VideoBufferStatus(int &Unused, int &Inuse, int &Held)
{
    return m_videoParent->Buffers()->GetBufferStatus(Unused, Inuse, Held);
}

void VideoDecoder::ProcessVideoPacket(AVStream *Stream, AVPacket *Packet)
{
    if (!Packet || !Stream || (Stream && !Stream->codec))
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

        m_currentPixelFormat    = Stream->codec->pix_fmt;
        m_currentVideoWidth     = Stream->codec->width;
        m_currentVideoHeight    = Stream->codec->height;
        m_currentReferenceCount = Stream->codec->refs;

        m_videoParent->Buffers()->FormatChanged(m_currentPixelFormat, m_currentVideoWidth, m_currentVideoHeight, m_currentReferenceCount);
    }

    // Add any preprocessing here

    // Decode a frame
    AVFrame avframe;
    avcodec_get_frame_defaults(&avframe);
    int gotframe = 0;

    int result = avcodec_decode_video2(Stream->codec, &avframe, &gotframe, Packet);

    if (result < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, "Unknown video decoding error");
        return;
    }

    if (!gotframe)
        return;

    VideoFrame *frame = avframe.opaque ? (VideoFrame*)avframe.opaque : m_videoParent->Buffers()->GetFrameForDecoding();

    if (!frame)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to get video frame");
        return;
    }

    frame->m_colorSpace    = Stream->codec->colorspace;
    frame->m_topFieldFirst = avframe.top_field_first;
    frame->m_interlaced    = avframe.interlaced_frame;
    frame->m_aspectRatio   = GetAspectRatio(Stream, avframe);
    frame->m_repeatPict    = avframe.repeat_pict;
    frame->m_frameNumber   = avframe.display_picture_number;
    frame->m_pts           = av_q2d(Stream->time_base) * 1000 * (avframe.pkt_pts - Stream->start_time);
    frame->m_dts           = av_q2d(Stream->time_base) * 1000 * (avframe.pkt_dts - Stream->start_time);

    m_videoParent->Buffers()->ReleaseFrameFromDecoding(frame);
}

void VideoDecoder::SetupVideoDecoder(AVStream *Stream)
{
    if (!Stream || (Stream && !Stream->codec))
        return;

    int threads = 1;

    AVCodecContext *context        = Stream->codec;
    context->thread_count          = threads;
    context->thread_safe_callbacks = 1;
    context->thread_type           = FF_THREAD_SLICE;
    context->draw_horiz_band       = NULL;
    context->slice_flags           = 0;
    context->err_recognition       = AV_EF_CRCCHECK | AV_EF_BITSTREAM;
    context->workaround_bugs       = FF_BUG_AUTODETECT;
    context->error_concealment     = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
    context->idct_algo             = FF_IDCT_AUTO;
    context->debug                 = 0;
    context->error_rate            = 0;
    context->opaque                = (void*)m_videoParent;
    context->get_buffer            = GetBuffer;
    context->release_buffer        = ReleaseBuffer;

    AVCodec *codec = avcodec_find_decoder(context->codec_id);

    if (codec && (codec->capabilities & CODEC_CAP_DR1))
    {
        context->flags            |= CODEC_FLAG_EMU_EDGE;
    }

    m_currentPixelFormat    = context->pix_fmt;
    m_currentVideoWidth     = context->width;
    m_currentVideoHeight    = context->height;
    m_currentReferenceCount = context->refs;

    m_videoParent->Buffers()->FormatChanged(context->pix_fmt, context->width, context->height, context->refs);
}

void VideoDecoder::FlushVideoBuffers(void)
{
    m_videoParent->Buffers()->Reset(false);
}

class VideoDecoderFactory : public DecoderFactory
{
    TorcDecoder* Create(int DecodeFlags, const QString &URI, TorcPlayer *Parent)
    {
        if (DecodeFlags & TorcDecoder::DecodeVideo)
            return new VideoDecoder(URI, Parent, DecodeFlags);

        return NULL;
    }
} VideoDecoderFactory;

