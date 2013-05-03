/*
 * VideoToolbox H264 hardware acceleration
 *
 * copyright (c) 2012 Sebastien Zwickert
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "h264.h"
#include "vt_internal.h"

static int start_frame(AVCodecContext *avctx,
                       av_unused const uint8_t *buffer,
                       av_unused uint32_t size)
{
    struct vt_context *vt_ctx = avctx->hwaccel_context;

    if (!vt_ctx->session)
        return -1;

    vt_ctx->priv_bitstream_size = 0;

    return 0;
}

static int decode_slice(AVCodecContext *avctx,
                        const uint8_t *buffer,
                        uint32_t size)
{
    struct vt_context *vt_ctx = avctx->hwaccel_context;
    void *tmp;

    if (!vt_ctx->session)
        return -1;

    tmp = av_fast_realloc(vt_ctx->priv_bitstream,
                          &vt_ctx->priv_allocated_size,
                          vt_ctx->priv_bitstream_size+size+4);
    if (!tmp)
        return AVERROR(ENOMEM);

    vt_ctx->priv_bitstream = tmp;

    AV_WB32(vt_ctx->priv_bitstream + vt_ctx->priv_bitstream_size, size);
    memcpy(vt_ctx->priv_bitstream + vt_ctx->priv_bitstream_size + 4, buffer, size);

    vt_ctx->priv_bitstream_size += size + 4;

    return 0;
}

static int end_frame(AVCodecContext *avctx)
{
    struct vt_context *vt_ctx = avctx->hwaccel_context;
    int status;

    if (!vt_ctx->session || !vt_ctx->priv_bitstream)
        return -1;

    status = ff_vt_session_decode_frame(vt_ctx);

    if (status) {
        av_log(avctx, AV_LOG_ERROR, "Failed to decode frame (%d)\n", status);
        return status;
    }

    ff_vt_end_frame(avctx->priv_data);

    return status;
}

AVHWAccel ff_h264_vt_hwaccel = {
    .name           = "h264_vt",
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = CODEC_ID_H264,
    .pix_fmt        = AV_PIX_FMT_VT_VLD,
    .start_frame    = start_frame,
    .decode_slice   = decode_slice,
    .end_frame      = end_frame,
};
