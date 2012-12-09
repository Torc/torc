/*
 * Copyright (c) 2010 Stefano Sabatini
 *
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * color source
 */

#include <stdio.h>
#include <string.h>

#include "avfilter.h"
#include "formats.h"
#include "internal.h"
#include "video.h"
#include "libavutil/pixdesc.h"
#include "libavutil/colorspace.h"
#include "libavutil/imgutils.h"
#include "libavutil/internal.h"
#include "libavutil/mathematics.h"
#include "libavutil/mem.h"
#include "libavutil/parseutils.h"
#include "drawutils.h"

typedef struct {
    int w, h;
    uint8_t color[4];
    AVRational time_base;
    uint8_t *line[4];
    int      line_step[4];
    int hsub, vsub;         ///< chroma subsampling values
    uint64_t pts;
} ColorContext;

static av_cold int color_init(AVFilterContext *ctx, const char *args)
{
    ColorContext *color = ctx->priv;
    char color_string[128] = "black";
    char frame_size  [128] = "320x240";
    char frame_rate  [128] = "25";
    AVRational frame_rate_q;
    int ret;

    if (args)
        sscanf(args, "%127[^:]:%127[^:]:%127s", color_string, frame_size, frame_rate);

    if (av_parse_video_size(&color->w, &color->h, frame_size) < 0) {
        av_log(ctx, AV_LOG_ERROR, "Invalid frame size: %s\n", frame_size);
        return AVERROR(EINVAL);
    }

    if (av_parse_video_rate(&frame_rate_q, frame_rate) < 0 ||
        frame_rate_q.den <= 0 || frame_rate_q.num <= 0) {
        av_log(ctx, AV_LOG_ERROR, "Invalid frame rate: %s\n", frame_rate);
        return AVERROR(EINVAL);
    }
    color->time_base.num = frame_rate_q.den;
    color->time_base.den = frame_rate_q.num;

    if ((ret = av_parse_color(color->color, color_string, -1, ctx)) < 0)
        return ret;

    return 0;
}

static av_cold void color_uninit(AVFilterContext *ctx)
{
    ColorContext *color = ctx->priv;
    int i;

    for (i = 0; i < 4; i++) {
        av_freep(&color->line[i]);
        color->line_step[i] = 0;
    }
}

static int query_formats(AVFilterContext *ctx)
{
    static const enum AVPixelFormat pix_fmts[] = {
        AV_PIX_FMT_ARGB,         AV_PIX_FMT_RGBA,
        AV_PIX_FMT_ABGR,         AV_PIX_FMT_BGRA,
        AV_PIX_FMT_RGB24,        AV_PIX_FMT_BGR24,

        AV_PIX_FMT_YUV444P,      AV_PIX_FMT_YUV422P,
        AV_PIX_FMT_YUV420P,      AV_PIX_FMT_YUV411P,
        AV_PIX_FMT_YUV410P,      AV_PIX_FMT_YUV440P,
        AV_PIX_FMT_YUVJ444P,     AV_PIX_FMT_YUVJ422P,
        AV_PIX_FMT_YUVJ420P,     AV_PIX_FMT_YUVJ440P,
        AV_PIX_FMT_YUVA420P,

        AV_PIX_FMT_NONE
    };

    ff_set_common_formats(ctx, ff_make_format_list(pix_fmts));
    return 0;
}

static int color_config_props(AVFilterLink *inlink)
{
    AVFilterContext *ctx = inlink->src;
    ColorContext *color = ctx->priv;
    uint8_t rgba_color[4];
    int is_packed_rgba;
    const AVPixFmtDescriptor *pix_desc = av_pix_fmt_desc_get(inlink->format);

    color->hsub = pix_desc->log2_chroma_w;
    color->vsub = pix_desc->log2_chroma_h;

    color->w &= ~((1 << color->hsub) - 1);
    color->h &= ~((1 << color->vsub) - 1);
    if (av_image_check_size(color->w, color->h, 0, ctx) < 0)
        return AVERROR(EINVAL);

    memcpy(rgba_color, color->color, sizeof(rgba_color));
    ff_fill_line_with_color(color->line, color->line_step, color->w, color->color,
                            inlink->format, rgba_color, &is_packed_rgba, NULL);

    av_log(ctx, AV_LOG_VERBOSE, "w:%d h:%d r:%d/%d color:0x%02x%02x%02x%02x[%s]\n",
           color->w, color->h, color->time_base.den, color->time_base.num,
           color->color[0], color->color[1], color->color[2], color->color[3],
           is_packed_rgba ? "rgba" : "yuva");
    inlink->w = color->w;
    inlink->h = color->h;
    inlink->time_base = color->time_base;

    return 0;
}

static int color_request_frame(AVFilterLink *link)
{
    ColorContext *color = link->src->priv;
    AVFilterBufferRef *picref = ff_get_video_buffer(link, AV_PERM_WRITE, color->w, color->h);
    AVFilterBufferRef *buf_out;
    int ret;

    if (!picref)
        return AVERROR(ENOMEM);

    picref->video->pixel_aspect = (AVRational) {1, 1};
    picref->pts                 = color->pts++;
    picref->pos                 = -1;

    buf_out = avfilter_ref_buffer(picref, ~0);
    if (!buf_out) {
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    ret = ff_start_frame(link, buf_out);
    if (ret < 0)
        goto fail;

    ff_draw_rectangle(picref->data, picref->linesize,
                      color->line, color->line_step, color->hsub, color->vsub,
                      0, 0, color->w, color->h);
    ret = ff_draw_slice(link, 0, color->h, 1);
    if (ret < 0)
        goto fail;

    ret = ff_end_frame(link);

fail:
    avfilter_unref_buffer(picref);

    return ret;
}

static const AVFilterPad avfilter_vsrc_color_outputs[] = {
    {
        .name          = "default",
        .type          = AVMEDIA_TYPE_VIDEO,
        .request_frame = color_request_frame,
        .config_props  = color_config_props
    },
    { NULL }
};

AVFilter avfilter_vsrc_color = {
    .name        = "color",
    .description = NULL_IF_CONFIG_SMALL("Provide an uniformly colored input, syntax is: [color[:size[:rate]]]"),

    .priv_size = sizeof(ColorContext),
    .init      = color_init,
    .uninit    = color_uninit,

    .query_formats = query_formats,

    .inputs    = NULL,

    .outputs   = avfilter_vsrc_color_outputs,
};
