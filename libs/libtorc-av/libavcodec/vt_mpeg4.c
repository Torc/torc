/*
 * VideoToolbox MPEG4 / H263 hardware acceleration
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

#include "vt_internal.h"

static int start_frame(AVCodecContext *avctx,
                       const uint8_t *buffer,
                       uint32_t size)
{
    struct vt_context *vt_ctx = avctx->hwaccel_context;

    if (!vt_ctx->session)
        return -1;

    return ff_vt_buffer_copy(vt_ctx, buffer, size);
}

static int decode_slice(AVCodecContext *avctx,
                        const uint8_t *buffer,
                        uint32_t size)
{
    struct vt_context *vt_ctx = avctx->hwaccel_context;

    if (!vt_ctx->session)
        return -1;

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

#if CONFIG_MPEG4_VT_HWACCEL
AVHWAccel ff_mpeg4_vt_hwaccel = {
    .name           = "mpeg4_vt",
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_MPEG4,
    .pix_fmt        = AV_PIX_FMT_VT_VLD,
    .start_frame    = start_frame,
    .end_frame      = end_frame,
    .decode_slice   = decode_slice,
};
#endif

#if CONFIG_H263_VT_HWACCEL
AVHWAccel ff_h263_vt_hwaccel = {
    .name           = "h263_vt",
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_H263,
    .pix_fmt        = AV_PIX_FMT_VT_VLD,
    .start_frame    = start_frame,
    .end_frame      = end_frame,
    .decode_slice   = decode_slice,
};
#endif

