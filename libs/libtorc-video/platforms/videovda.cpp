/* Class VideoVDA
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

// Torc
#include "torclogging.h"
#include "videoplayer.h"
#include "videodecoder.h"
#include "videovda.h"

bool VideoVDA::InitialiseDecoder(AVCodecContext *Context, AVPixelFormat Format)
{
    if (Format == AV_PIX_FMT_VDA_VLD && Context->codec_id == AV_CODEC_ID_H264)
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
            return true;
        }
    }

    return false;
}

class VDAAcceleration : public AccelerationFactory
{
  public:
    VDAAcceleration() : AccelerationFactory()
    {
        TorcCommandLine::RegisterEnvironmentVariable("TORC_NO_VDA", "Disable VDA video hardware acceleration.");
    }

    bool InitialiseDecoder(AVCodecContext *Context, AVPixelFormat Format)
    {
        if (!qgetenv("TORC_NO_VDA").isEmpty())
            return false;

        if (VideoPlayer::gAllowOtherAcceleration && VideoPlayer::gAllowOtherAcceleration->IsActive() &&
            VideoPlayer::gAllowOtherAcceleration->GetValue().toBool())
        {
            return VideoVDA::InitialiseDecoder(Context, Format);
        }

        return false;
    }

    void DeinitialiseDecoder(AVCodecContext *Context)
    {
        if (Context && Context->hwaccel_context && Context->pix_fmt == AV_PIX_FMT_VDA_VLD)
        {
            vda_context *context = static_cast<vda_context*>(Context->hwaccel_context);
            if (context)
            {
                LOG(VB_PLAYBACK, LOG_INFO, "Destroying VDA decoder");
                ff_vda_destroy_decoder(context);
                delete context;
                Context->hwaccel_context = NULL;
            }
        }
    }

    bool InitialiseBuffer(AVCodecContext*, AVFrame*, VideoFrame*)
    {
        return false;
    }

    void DeinitialiseBuffer(AVCodecContext *Context, AVFrame *Avframe, VideoFrame *Frame)
    {
        if (Context && Avframe && Avframe->format == AV_PIX_FMT_VDA_VLD && Context->hwaccel_context)
        {
            CVPixelBufferRef buffer = (CVPixelBufferRef)Avframe->data[3];
            if (buffer)
                CFRelease(buffer);
        }
    }

    void ConvertBuffer(AVFrame &Avframe, VideoFrame *Frame, SwsContext *&ConversionContext)
    {
        if (Avframe.format == AV_PIX_FMT_VDA_VLD && Frame->m_pixelFormat == AV_PIX_FMT_VDA_VLD)
        {
            CVPixelBufferRef buffer = (CVPixelBufferRef)Avframe.data[3];
            if (buffer)
            {
                CVPixelBufferLockBaseAddress(buffer, 0);

                int width  = Frame->m_rawWidth;
                int height = Frame->m_rawHeight;

                AVPicture in;
                memset(&in, 0, sizeof(AVPicture));

                uint planes = CVPixelBufferGetPlaneCount(buffer);
                if (planes)
                {
                    for (uint i = 0; i < planes; ++i)
                    {
                        in.data[i]     = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(buffer, i);
                        in.linesize[i] = CVPixelBufferGetBytesPerRowOfPlane(buffer, i);
                    }
                }
                else
                {
                    in.data[0]     = (uint8_t*)CVPixelBufferGetBaseAddress(buffer);
                    in.linesize[0] = CVPixelBufferGetBytesPerRow(buffer);
                }

                AVPicture out;
                avpicture_fill(&out, Frame->m_buffer, Frame->m_secondaryPixelFormat, Frame->m_rawWidth, Frame->m_rawHeight);

                ConversionContext = sws_getCachedContext(ConversionContext,
                                                         width, height, VDA_PIX_FORMAT,
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

    bool UpdateFrame(VideoFrame*F, VideoColourSpace*, void*, void*)
    {
        return false;
    }

    bool ReleaseFrame(VideoFrame*)
    {
        return false;
    }

    bool NeedsCustomSurfaceFormat(VideoFrame*, void*)
    {
        return false;
    }

    bool SupportedProperties(VideoFrame*, QSet<TorcPlayer::PlayerProperty>&)
    {
        // support the defaults (VDA frame is returned as a software frame)
        return false;
    }

} VDAAcceleration;
