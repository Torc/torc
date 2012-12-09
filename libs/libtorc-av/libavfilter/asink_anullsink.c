/*
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

#include "libavutil/internal.h"
#include "avfilter.h"
#include "internal.h"

static int null_filter_samples(AVFilterLink *link, AVFilterBufferRef *samplesref)
{
    return 0;
}

static const AVFilterPad avfilter_asink_anullsink_inputs[] = {
    {
        .name           = "default",
        .type           = AVMEDIA_TYPE_AUDIO,
        .filter_samples = null_filter_samples,
    },
    { NULL },
};

AVFilter avfilter_asink_anullsink = {
    .name        = "anullsink",
    .description = NULL_IF_CONFIG_SMALL("Do absolutely nothing with the input audio."),

    .priv_size = 0,

    .inputs    = avfilter_asink_anullsink_inputs,
    .outputs   = NULL,
};
