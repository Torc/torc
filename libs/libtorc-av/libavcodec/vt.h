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

#ifndef AVCODEC_VT_H
#define AVCODEC_VT_H

/**
 * @file
 * @ingroup lavc_codec_hwaccel_vt
 * Public libavcodec VideoToolbox header.
 */

#include <stdint.h>

#define Picture QuickdrawPicture
#include <VideoToolbox/VideoToolbox.h>
#undef Picture

/**
 * This structure is used to provide the necessary configurations and data
 * to the VideoToolbox Libav HWAccel implementation.
 *
 * The application must make it available as AVCodecContext.hwaccel_context.
 */
struct vt_context {
    /**
     * VideoToolbox decompression session.
     *
     * - encoding: unused.
     * - decoding: Set/Unset by libavcodec.
     */
    VTDecompressionSessionRef   session;

    /**
     * The width of encoded video.
     *
     * - encoding: unused.
     * - decoding: Set/Unset by user.
     */
    int                 width;

    /**
     * The height of encoded video.
     *
     * - encoding: unused.
     * - decoding: Set/Unset by user.
     */
    int                 height;

    /**
     * The type of video compression.
     *
     * - encoding: unused.
     * - decoding: Set/Unset by user.
     */
    int                 cm_codec_type;

    /**
     * The pixel format for output image buffers.
     *
     * - encoding: unused.
     * - decoding: Set/Unset by user.
     */
    OSType              cv_pix_fmt;

    /**
     * The video format description.
     *
     * encoding: unused.
     * decoding: Set by user. Unset by libavcodec.
     */
    CMVideoFormatDescriptionRef cm_fmt_desc;

    /**
     * The Core Video pixel buffer that contains the current image data.
     *
     * encoding: unused.
     * decoding: Set by libavcodec. Unset by user.
     */
    CVImageBufferRef	cv_buffer;

    /**
     * The current bitstream buffer.
     */
    uint8_t             *priv_bitstream;

    /**
     * The current size of the bitstream.
     */
    int                 priv_bitstream_size;

    /**
     * The allocated size used for fast reallocation.
     */
    int                 priv_allocated_size;
};

/**
 * Create a decompression session.
 *
 * @param vt_ctx        The VideoToolbox context.
 * @param fmt_desc      The video format description.
 * @param decoder_spec  The specific decoder configuration.
 * @param buf_attr      The output buffer attributes.
 * @return Status.
 */
int ff_vt_session_create(struct vt_context *vt_ctx,
                         CMVideoFormatDescriptionRef fmt_desc,
                         CFDictionaryRef decoder_spec,
						 CFDictionaryRef buf_attr);

/**
 * Destroy a decompression session.
 *
 * @param vt_ctx        The VideoToolbox context.
 */
void ff_vt_session_invalidate(struct vt_context *vt_ctx);

/**
 * Create the decoder configuration dictionary.
 *
 * It is the user's responsability to release the returned object (using CFRelease) when
 * user has finished with it.
 *
 * @param codec_type    The type of video compression.
 * @param extradata     The stream extradata.
 * @param size          The extradata size.
 * @return  A CoreFoundation dictionary or NULL if there was a problem creating the object.
 */
CFDictionaryRef ff_vt_decoder_config_create(CMVideoCodecType codec_type,
                                            uint8_t *extradata, int size);

/**
 * Create the output buffer attributes.
 *
 * It is the user's responsability to release the returned object (using CFRelease) when
 * user has finished with it.
 *
 * @param width         The width of output image.
 * @param height        The height of output image.
 * @param pix_fmt       The pixel format of output image.
 * @return  A CoreFoundation dictionary.
 */
CFDictionaryRef ff_vt_buffer_attributes_create(int width, int height, OSType pix_fmt);

/**
 * Create the video format description.
 *
 * It is the user's responsability to release the returned object (using CFRelease) when
 * user has finished with it.
 *
 * @param codec_type    The type of video compression.
 * @param decoder_spec  The specific decoder configuration.
 * @param width         The width of encoded video.
 * @param height        The height of encoded video.
 * @return  A CoreMedia video format object.
 */
CMVideoFormatDescriptionRef ff_vt_format_desc_create(CMVideoCodecType codec_type,
                                                     CFDictionaryRef decoder_spec,
                                                     int width, int height);

/**
 * @}
 */

#endif /* AVCODEC_VT_H */
