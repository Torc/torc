/*
 * VideoToolbox hardware acceleration
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

#ifndef AVCODEC_VT_INTERNAL_H
#define AVCODEC_VT_INTERNAL_H

#include "vt.h"
#include "mpegvideo.h"

/**
 * @addtogroup VT_Decoding
 *
 * @{
 */

int ff_vt_session_decode_frame(struct vt_context *vt_ctx);

int ff_vt_buffer_copy(struct vt_context *vt_ctx,
                      const uint8_t *buffer,
                      uint32_t size);
void ff_vt_end_frame(MpegEncContext *s);

/* @} */

#endif /* AVCODEC_VT_INTERNAL_H */
