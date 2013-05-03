/* Class VideoVT
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
#include "torclogging.h"
#include "videoplayer.h"
#include "videodecoder.h"
#include "videovt.h"

/*! \class VideoVT
 *  \brief Video decoder acceleration using the OS X VideToolbox framework
 *
 * \todo H264 crashes
 * \todo H263 untested
 * \todo Deinitialisation not working as expected
*/

bool VideoVT::InitialiseDecoder(AVCodecContext *Context, AVPixelFormat Format)
{
    if ((Context->codec_id == AV_CODEC_ID_MPEG4 || Context->codec_id == AV_CODEC_ID_MPEG2VIDEO ||
         Context->codec_id == AV_CODEC_ID_H263  || Context->codec_id == AV_CODEC_ID_H264) &&
        Context->extradata_size && Format == AV_PIX_FMT_VT_VLD)
    {
        CMVideoCodecType          codectype = Context->codec_id == AV_CODEC_ID_MPEG4 ? kCMVideoCodecType_MPEG4Video :
                                              Context->codec_id == AV_CODEC_ID_MPEG2VIDEO ? kCMVideoCodecType_MPEG2Video :
                                              Context->codec_id == AV_CODEC_ID_H263 ? kCMVideoCodecType_H263 : kCMVideoCodecType_H264;
        CFDictionaryRef         decoderspec = ff_vt_decoder_config_create(codectype, Context->extradata, Context->extradata_size);
        CMVideoFormatDescriptionRef fmtdesc = ff_vt_format_desc_create(codectype, decoderspec, Context->width, Context->height);
        CFDictionaryRef    bufferattributes = ff_vt_buffer_attributes_create(Context->width, Context->height, VT_CV_FORMAT);

        struct vt_context *context = new vt_context;
        memset(context, 0, sizeof(vt_context));
        context->width             = Context->width;
        context->height            = Context->height;
        context->cv_pix_fmt        = VT_CV_FORMAT;
        context->cm_fmt_desc       = fmtdesc;
        context->cm_codec_type     = codectype;

        if (ff_vt_session_create(context, fmtdesc, decoderspec, bufferattributes))
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to create VT context");
            delete context;
            return false;
        }

        LOG(VB_GENERAL, LOG_ERR, "Created VT context");
        Context->hwaccel_context = context;
        return true;
    }

    return false;
}

class VTAcceleration : public AccelerationFactory
{
    bool InitialiseDecoder(AVCodecContext *Context, AVPixelFormat Format)
    {
        if (VideoPlayer::gAllowOtherAcceleration && VideoPlayer::gAllowOtherAcceleration->IsActive() &&
            VideoPlayer::gAllowOtherAcceleration->GetValue().toBool())
        {
            return VideoVT::InitialiseDecoder(Context, Format);
        }

        return false;
    }

    void DeinitialiseDecoder(AVCodecContext *Context)
    {
        if (Context && Context->hwaccel_context && Context->pix_fmt == AV_PIX_FMT_VT_VLD)
        {
            // FIXME - this cast isn't working...
            vt_context *context = (vt_context*)Context->hwaccel_context;
            if (context)
            {
                LOG(VB_PLAYBACK, LOG_INFO, "Destroying VT decoder");
                CFRelease(context->cm_fmt_desc);
                delete context;
                Context->hwaccel_context = NULL;
            }
        }
    }

    bool InitialiseBuffer(AVCodecContext *Context, AVFrame *Avframe, VideoFrame *Frame)
    {
        return false;
    }

    void DeinitialiseBuffer(AVCodecContext *Context, AVFrame *Avframe, VideoFrame *Frame)
    {
        if (Context && Avframe && Avframe->format == AV_PIX_FMT_VT_VLD && Context->hwaccel_context)
        {
            CVImageBufferRef buffer = (CVImageBufferRef)Avframe->data[3];
            if (buffer)
                CFRelease(buffer);
        }
    }

    void ConvertBuffer(AVFrame &Avframe, VideoFrame *Frame, SwsContext *&ConversionContext)
    {
        if (Avframe.format == AV_PIX_FMT_VT_VLD && Frame->m_pixelFormat == AV_PIX_FMT_VT_VLD)
        {
            CVImageBufferRef buffer = (CVImageBufferRef)Avframe.data[3];
            if (buffer)
            {
                CVPixelBufferLockBaseAddress(buffer, 0);

                int width  = Frame->m_rawWidth;
                int height = Frame->m_rawHeight;

                AVPicture in;
                memset(&in, 0, sizeof(AVPicture));
                uint planes = std::max((uint)1, (uint)CVPixelBufferGetPlaneCount(buffer));
                for (uint i = 0; i < planes; ++i)
                {
                    in.data[i]     = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(buffer, i);
                    in.linesize[i] = CVPixelBufferGetBytesPerRowOfPlane(buffer, i);
                }

                AVPicture out;
                avpicture_fill(&out, Frame->m_buffer, Frame->m_secondaryPixelFormat, Frame->m_rawWidth, Frame->m_rawHeight);

                ConversionContext = sws_getCachedContext(ConversionContext,
                                                         width, height, VT_PIX_FORMAT,
                                                         width, height, Frame->m_secondaryPixelFormat,
                                                         SWS_FAST_BILINEAR, NULL, NULL, NULL);
                if (ConversionContext != NULL)
                {
                    if (sws_scale(ConversionContext, in.data, in.linesize, 0, height, out.data, out.linesize) < 1)
                        LOG(VB_GENERAL, LOG_ERR, "Software scaling/conversion failed");
                }
                else
                {
                    LOG(VB_GENERAL, LOG_ERR, "Failed to create software conversion context");
                }

                CVPixelBufferUnlockBaseAddress(buffer, 0);
            }
        }
    }

    bool UpdateFrame(VideoFrame *Frame, VideoColourSpace *ColourSpace, void *Surface)
    {
        (void)Frame;
        (void)ColourSpace;
        (void)Surface;
        return false;
    }

    bool ReleaseFrame(VideoFrame *Frame)
    {
        (void)Frame;
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
        (void)Frame;
        (void)Format;
        return false;
    }

    bool SupportedProperties(VideoFrame *Frame, QSet<TorcPlayer::PlayerProperty> &Properties)
    {
        // support the defaults (VT frame is returned as a software frame)
        (void)Frame;
        (void)Properties;
        return false;
    }

} VTAcceleration;
