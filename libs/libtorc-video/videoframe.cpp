/* Class VideoFrame
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
#include "videoframe.h"

extern "C" {
#include "libavutil/mem.h"
#include "libavutil/pixdesc.h"
}

/*! \class VideoFrame
 *  \brief A simple video frame storage class.
 *
 * \sa VideoBuffers
 *
 * \todo Convert more initialisation code to use libav functions.
*/

VideoFrame::VideoFrame()
{
    Reset();
}

VideoFrame::~VideoFrame()
{
    Reset();
}

void VideoFrame::Reset(void)
{
    m_discard         = false;
    m_rawWidth        = 0;
    m_rawHeight       = 0;
    m_adjustedWidth   = 0;
    m_adjustedHeight  = 0;
    m_bitsPerPixel    = 0;
    m_numPlanes       = 0;
    m_pixelFormat     = PIX_FMT_NONE;
    m_colorSpace      = AVCOL_SPC_BT709;
    m_buffer          = NULL;
    m_topFieldFirst   = false;
    m_interlaced      = false;
    m_aspectRatio     = 0.0;
    m_repeatPict      = false;
    m_frameNumber     = 0;
    m_pts             = AV_NOPTS_VALUE;
    m_dts             = AV_NOPTS_VALUE;
    m_pitches[0]      = m_pitches[1] = m_pitches[2] = m_pitches[4] = 0;
    m_offsets[0]      = m_offsets[1] = m_offsets[2] = m_offsets[4] = 0;
    m_priv[0]         = m_priv[1] = m_priv[2] = m_priv[3] = NULL;
    if (m_buffer)
        av_free(m_buffer);
    m_buffer = NULL;
}

bool VideoFrame::Initialise(PixelFormat Format, int Width, int Height)
{
    if (m_pixelFormat != PIX_FMT_NONE)
        Reset();

    if (Height < 1 || Width < 1 || Format == PIX_FMT_NONE)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Cannot initialise frame (%1 %2x%3")
            .arg(av_get_pix_fmt_name(Format)).arg(Width).arg(Height));
        return false;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Initialise frame: '%1' %2x%3")
        .arg(av_get_pix_fmt_name(Format)).arg(Width).arg(Height));

    m_rawWidth        = Width;
    m_rawHeight       = Height;
    m_adjustedWidth   = (Width  + 15) & ~0xf;
    m_adjustedHeight  = (Height + 15) & ~0xf;
    m_pixelFormat     = Format;
    m_bitsPerPixel    = av_get_bits_per_pixel(&av_pix_fmt_descriptors[Format]);
    m_numPlanes       = PlaneCount(Format);
    m_buffer          = (unsigned char*)av_malloc((m_adjustedWidth * m_adjustedHeight * m_bitsPerPixel) >> 3);

    // FIXME this will fail for HW frames
    return m_buffer && (m_pixelFormat != PIX_FMT_NONE) && m_adjustedWidth &&
           m_adjustedHeight && m_bitsPerPixel && m_numPlanes && SetOffsets();
}

bool VideoFrame::Discard(void)
{
    return m_discard;
}

void VideoFrame::SetDiscard(void)
{
    m_discard = true;
}

int VideoFrame::PlaneCount(PixelFormat Format)
{
    switch (Format)
    {
        case PIX_FMT_YUV420P:
        case PIX_FMT_YUV411P:
        case PIX_FMT_YUV422P:
        case PIX_FMT_YUV444P:
            return 3;
        case PIX_FMT_NV12:
        case PIX_FMT_NV21:
            return 2;
        case PIX_FMT_UYYVYY411:
        case PIX_FMT_UYVY422:
        case PIX_FMT_YUYV422:
        case PIX_FMT_ARGB:
        case PIX_FMT_RGBA:
        case PIX_FMT_ABGR:
        case PIX_FMT_BGRA:
        case PIX_FMT_RGB24:
        case PIX_FMT_BGR24:
        case PIX_FMT_GRAY8:
        case PIX_FMT_BGR8:
        case PIX_FMT_BGR4_BYTE:
        case PIX_FMT_RGB8:
        case PIX_FMT_RGB4_BYTE:
        case PIX_FMT_BGR4:
        case PIX_FMT_RGB4:
        case PIX_FMT_MONOWHITE:
        case PIX_FMT_MONOBLACK:
        case PIX_FMT_GRAY16BE:
        case PIX_FMT_GRAY16LE:
            return 1;
        default:
            return 0; // hardware format or unsupported
    }
}

bool VideoFrame::SetOffsets(void)
{
    int halfpitch = m_adjustedWidth >> 1;
    int fullplane = m_adjustedWidth * m_adjustedHeight;

    m_offsets[0] = 0;
    m_pitches[0] = m_adjustedWidth;

    if (m_numPlanes == 1)
    {
        m_offsets[1] = m_offsets[2] = m_offsets[3] = m_offsets[0];
        m_pitches[1] = m_pitches[2] = m_pitches[3] = m_pitches[0];
        return true;
    }

    if (m_numPlanes == 2)
    {
        m_offsets[1] = m_offsets[2] = m_offsets[3] = fullplane;
        m_pitches[1] = m_pitches[2] = m_pitches[3] = halfpitch;
        return true;
    }

    if (m_numPlanes != 3)
        return false;

    if (m_pixelFormat == PIX_FMT_YUV420P)
    {
        m_offsets[1] = fullplane;
        m_offsets[2] = m_offsets[3] = m_offsets[1] + fullplane >> 2;
        m_pitches[1] = m_pitches[2] = m_pitches[3] = halfpitch;
    }
    else if (m_pixelFormat == PIX_FMT_YUV411P)
    {
        m_offsets[1] = fullplane;
        m_offsets[2] = m_offsets[3] = m_offsets[1] + fullplane >> 2;
        m_pitches[1] = m_pitches[2] = m_pitches[3] = halfpitch >> 1;
    }
    else if (m_pixelFormat == PIX_FMT_YUV422P)
    {
        m_offsets[1] = fullplane;
        m_offsets[2] = m_offsets[3] = m_offsets[1] + fullplane >> 1;
        m_pitches[1] = m_pitches[2] = m_pitches[3] = halfpitch;
    }
    else if (m_pixelFormat == PIX_FMT_YUV444P)
    {
        m_offsets[1] = fullplane;
        m_offsets[2] = m_offsets[3] = m_offsets[1] + fullplane;
        m_pitches[1] = m_pitches[2] = m_pitches[3] = m_pitches[0];
    }
    else
    {
        return false;
    }

    return true;
}
