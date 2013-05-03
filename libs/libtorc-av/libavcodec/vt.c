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

#include "libavutil/avutil.h"
#include "vt_internal.h"

static int vt_write_mp4_descr_length(PutBitContext *pb, int length, int compact)
{
    int i;
    uint8_t b;
    int nb;

    if (compact) {
        if (length <= 0x7F)
            nb = 1;
        else if (length <= 0x3FFF)
             nb = 2;
        else if (length <= 0x1FFFFF)
            nb = 3;
        else
            nb = 4;
    }
    else
        nb = 4;

    for (i = nb-1; i >= 0; i--) {
        b = (length >> (i * 7)) & 0x7F;
        if (i != 0) {
            b |= 0x80;
        }
        put_bits(pb, 8, b);
    }

    return nb;
}

static CFDataRef vt_esds_extradata_create(uint8_t *extradata, int size)
{
    CFDataRef data;
    uint8_t *rw_extradata;
    PutBitContext pb;
    int full_size = 3 + (5 + (13 + (5 + size))) + 3;
    int config_size = 13 + 5 + size;
    int padding = 12;
    int s, i = 0;

    if (!(rw_extradata = av_mallocz(sizeof(uint8_t)*(full_size+padding))))
        return NULL;

    init_put_bits(&pb, rw_extradata, full_size+padding);
    put_bits(&pb, 8, 0);        ///< version
    put_bits(&pb, 24, 0);       ///< flags

    // elementary stream descriptor
    put_bits(&pb, 8, 0x03);     ///< ES_DescrTag
    vt_write_mp4_descr_length(&pb, full_size, 0);
    put_bits(&pb, 16, 0);       ///< esid
    put_bits(&pb, 8, 0);        ///< stream priority (0-32)

    // decoder configuration descriptor
    put_bits(&pb, 8, 0x04);     ///< DecoderConfigDescrTag
    vt_write_mp4_descr_length(&pb, config_size, 0);
    put_bits(&pb, 8, 32);       ///< object type indication. 32 = CODEC_ID_MPEG4
    put_bits(&pb, 8, 0x11);     ///< stream type
    put_bits(&pb, 24, 0);       ///< buffer size
    put_bits32(&pb, 0);         ///< max bitrate
    put_bits32(&pb, 0);         ///< avg bitrate

    // decoder specific descriptor
    put_bits(&pb, 8, 0x05);     ///< DecSpecificInfoTag
    vt_write_mp4_descr_length(&pb, size, 0);
    for (i = 0; i < size; i++)
        put_bits(&pb, 8, extradata[i]);

    // SLConfigDescriptor
    put_bits(&pb, 8, 0x06);     ///< SLConfigDescrTag
    put_bits(&pb, 8, 0x01);     ///< length
    put_bits(&pb, 8, 0x02);     ///<

    flush_put_bits(&pb);
    s = put_bits_count(&pb) / 8;

    data = CFDataCreate(kCFAllocatorDefault, rw_extradata, s);

    av_freep(&rw_extradata);
    return data;
}

static CFDataRef vt_avcc_extradata_create(uint8_t *extradata, int size)
{
    CFDataRef data;
    /* Each VCL NAL in the bistream sent to the decoder
     * is preceded by a 4 bytes length header.
     * Change the avcC atom header if needed, to signal headers of 4 bytes. */
    if (size >= 4 && (extradata[4] & 0x03) != 0x03) {
        uint8_t *rw_extradata;

        if (!(rw_extradata = av_malloc(size)))
            return NULL;

        memcpy(rw_extradata, extradata, size);

        rw_extradata[4] |= 0x03;

        data = CFDataCreate(kCFAllocatorDefault, rw_extradata, size);

        av_freep(&rw_extradata);
    } else {
        data = CFDataCreate(kCFAllocatorDefault, extradata, size);
    }

    return data;
}

static CMSampleBufferRef vt_sample_buffer_create(CMFormatDescriptionRef fmt_desc,
                                                 void *buffer,
                                                 int size)
{
    OSStatus status;
    CMBlockBufferRef  block_buf;
    CMSampleBufferRef sample_buf;

    block_buf  = NULL;
    sample_buf = NULL;

    status = CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault,///< structureAllocator
                                                buffer,             ///< memoryBlock
                                                size,               ///< blockLength
                                                kCFAllocatorNull,   ///< blockAllocator
                                                NULL,               ///< customBlockSource
                                                0,                  ///< offsetToData
                                                size,               ///< dataLength
                                                0,                  ///< flags
                                                &block_buf);

    if (!status) {
        status = CMSampleBufferCreate(kCFAllocatorDefault,  ///< allocator
                                      block_buf,            ///< dataBuffer
                                      TRUE,                 ///< dataReady
                                      0,                    ///< makeDataReadyCallback
                                      0,                    ///< makeDataReadyRefcon
                                      fmt_desc,             ///< formatDescription
                                      1,                    ///< numSamples
                                      0,                    ///< numSampleTimingEntries
                                      NULL,                 ///< sampleTimingArray
                                      0,                    ///< numSampleSizeEntries
                                      NULL,                 ///< sampleSizeArray
                                      &sample_buf);
    }

    if (block_buf)
        CFRelease(block_buf);

    return sample_buf;
}

static void vt_decoder_callback(void *vt_hw_ctx,
                                void *sourceFrameRefCon,
                                OSStatus status,
                                VTDecodeInfoFlags flags,
                                CVImageBufferRef image_buffer,
                                CMTime pts,
                                CMTime duration)
{
    struct vt_context *vt_ctx = vt_hw_ctx;
    vt_ctx->cv_buffer = NULL;

    if (!image_buffer)
        return;

    if (vt_ctx->cv_pix_fmt != CVPixelBufferGetPixelFormatType(image_buffer))
        return;

    vt_ctx->cv_buffer = CVPixelBufferRetain(image_buffer);
}

int ff_vt_session_decode_frame(struct vt_context *vt_ctx)
{
    OSStatus status;
    CMSampleBufferRef sample_buf;

    sample_buf = vt_sample_buffer_create(vt_ctx->cm_fmt_desc,
                                         vt_ctx->priv_bitstream,
                                         vt_ctx->priv_bitstream_size);

    if (!sample_buf)
        return -1;

    status = VTDecompressionSessionDecodeFrame(vt_ctx->session,
                                               sample_buf,
                                               0,               ///< decodeFlags
                                               NULL,            ///< sourceFrameRefCon
                                               0);              ///< infoFlagsOut
    if (status == noErr)
        status = VTDecompressionSessionWaitForAsynchronousFrames(vt_ctx->session);

    CFRelease(sample_buf);

    return status;
}

int ff_vt_buffer_copy(struct vt_context *vt_ctx, const uint8_t *buffer, uint32_t size)
{
    void *tmp;
    tmp = av_fast_realloc(vt_ctx->priv_bitstream,
                          &vt_ctx->priv_allocated_size,
                          size);
    if (!tmp)
        return AVERROR(ENOMEM);

    vt_ctx->priv_bitstream = tmp;

    memcpy(vt_ctx->priv_bitstream, buffer, size);

    vt_ctx->priv_bitstream_size = size;

    return 0;
}

void ff_vt_end_frame(MpegEncContext *s)
{
    struct vt_context * const vt_ctx = s->avctx->hwaccel_context;
    AVFrame *frame = &s->current_picture_ptr->f;
    frame->data[3] = (void*)vt_ctx->cv_buffer;
}

int ff_vt_session_create(struct vt_context *vt_ctx,
                         CMVideoFormatDescriptionRef fmt_desc,
                         CFDictionaryRef decoder_spec,
						 CFDictionaryRef buf_attr)
{
    OSStatus status;
    VTDecompressionOutputCallbackRecord decoder_cb;

    if (!fmt_desc)
        return -1;

    vt_ctx->cm_fmt_desc = CFRetain(fmt_desc);

    decoder_cb.decompressionOutputCallback = vt_decoder_callback;
    decoder_cb.decompressionOutputRefCon = vt_ctx;

    status = VTDecompressionSessionCreate(NULL,             ///< allocator
                                          fmt_desc,         ///< videoFormatDescription
                                          decoder_spec,     ///< videoDecoderSpecification
                                          buf_attr,         ///< destinationImageBufferAttributes
                                          &decoder_cb,      ///< outputCallback
                                          &vt_ctx->session);///< decompressionSessionOut
    if (noErr != status)
        return status;

    return 0;
}

void ff_vt_session_invalidate(struct vt_context *vt_ctx)
{
    if (vt_ctx->cm_fmt_desc)
        CFRelease(vt_ctx->cm_fmt_desc);

    if (vt_ctx->session)
        VTDecompressionSessionInvalidate(vt_ctx->session);

    av_freep(&vt_ctx->priv_bitstream);
}

CFDictionaryRef ff_vt_decoder_config_create(CMVideoCodecType codec_type,
                                            uint8_t *extradata, int size)
{
    CFMutableDictionaryRef config_info = NULL;
    if (size) {
        CFMutableDictionaryRef avc_info;
        CFDataRef data;

        config_info = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                1,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks);
        avc_info = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                             1,
                                             &kCFTypeDictionaryKeyCallBacks,
                                             &kCFTypeDictionaryValueCallBacks);

        switch (codec_type) {
            case kCMVideoCodecType_MPEG4Video :
                data = vt_esds_extradata_create(extradata, size);
                if (data)
                    CFDictionarySetValue(avc_info, CFSTR("esds"), data);
                break;
            case kCMVideoCodecType_H264 :
                data = vt_avcc_extradata_create(extradata, size);
                if (data)
                    CFDictionarySetValue(avc_info, CFSTR("avcC"), data);
                break;
            default:
                break;
        }

        CFDictionarySetValue(config_info,
                             kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms,
                             avc_info);

        if (data)
            CFRelease(data);
        CFRelease(avc_info);
    }
    return config_info;
}

CFDictionaryRef ff_vt_buffer_attributes_create(int width, int height, OSType pix_fmt)
{
    CFMutableDictionaryRef buffer_attributes;
    CFMutableDictionaryRef io_surface_properties;
    CFNumberRef cv_pix_fmt;
    CFNumberRef w;
    CFNumberRef h;

    w = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &width);
    h = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &height);
    cv_pix_fmt = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pix_fmt);

    buffer_attributes = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                  4,
                                                  &kCFTypeDictionaryKeyCallBacks,
                                                  &kCFTypeDictionaryValueCallBacks);
    io_surface_properties = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                      0,
                                                      &kCFTypeDictionaryKeyCallBacks,
                                                      &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(buffer_attributes,
                         kCVPixelBufferPixelFormatTypeKey,
                         cv_pix_fmt);
    CFDictionarySetValue(buffer_attributes,
                         kCVPixelBufferIOSurfacePropertiesKey,
                         io_surface_properties);
    CFDictionarySetValue(buffer_attributes,
                         kCVPixelBufferWidthKey,
                         w);
    CFDictionarySetValue(buffer_attributes,
                         kCVPixelBufferHeightKey,
                         h);

    CFRelease(io_surface_properties);
    CFRelease(cv_pix_fmt);
    CFRelease(w);
    CFRelease(h);

    return buffer_attributes;
}

CMVideoFormatDescriptionRef ff_vt_format_desc_create(CMVideoCodecType codec_type,
                                                     CFDictionaryRef decoder_spec,
                                                     int width, int height)
{
    CMFormatDescriptionRef cm_fmt_desc;
    OSStatus status;

    status = CMVideoFormatDescriptionCreate(kCFAllocatorDefault,
                                            codec_type,
                                            width,
                                            height,
                                            decoder_spec, ///< Dictionary of extension
                                            &cm_fmt_desc);

    if (status)
        return NULL;

    return cm_fmt_desc;
}
