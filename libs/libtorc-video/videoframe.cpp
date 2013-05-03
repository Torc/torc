/* Class VideoFrame
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
#include "videodecoder.h"
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

VideoFrame::VideoFrame(AVPixelFormat PreferredDisplayFormat)
  : m_preferredDisplayFormat(PreferredDisplayFormat)
{
    m_buffer               = NULL;
    m_acceleratedBuffer    = NULL;
    Reset();
}

VideoFrame::~VideoFrame()
{
    Reset();
}

void VideoFrame::Reset(void)
{
    // delete before pixel format is reset
    if (m_acceleratedBuffer)
    {
        AccelerationFactory* factory = AccelerationFactory::GetAccelerationFactory();
        for ( ; factory; factory = factory->NextFactory())
            if (factory->ReleaseFrame(this))
                break;
    }
    m_acceleratedBuffer    = NULL;

    m_discard              = false;
    m_rawWidth             = 0;
    m_rawHeight            = 0;
    m_adjustedWidth        = 0;
    m_adjustedHeight       = 0;
    m_bitsPerPixel         = 0;
    m_numPlanes            = 0;
    m_bufferSize           = 0;
    m_pixelFormat          = AV_PIX_FMT_NONE;
    m_secondaryPixelFormat = AV_PIX_FMT_NONE;
    m_invertForSource     = 0;
    m_invertForDisplay     = 0;
    m_colourSpace          = AVCOL_SPC_BT709;
    m_colourRange          = AVCOL_RANGE_UNSPECIFIED;
    m_field                = Frame;
    m_topFieldFirst        = false;
    m_interlaced           = false;
    m_frameAspectRatio     = 0.0;
    m_pixelAspectRatio     = 0.0;
    m_repeatPict           = false;
    m_frameNumber          = 0;
    m_pts                  = AV_NOPTS_VALUE;
    m_corrupt              = false;
    m_frameRate            = 30000.0f / 1001.0f;
    m_pitches[0]           = m_pitches[1] = m_pitches[2] = m_pitches[3] = 0;
    m_offsets[0]           = m_offsets[1] = m_offsets[2] = m_offsets[3] = 0;
    m_priv[0]              = m_priv[1] = m_priv[2] = m_priv[3] = NULL;
    if (m_buffer)
        av_free(m_buffer);
    m_buffer = NULL;
}

void VideoFrame::Initialise(AVPixelFormat Format, int Width, int Height)
{
    if (m_pixelFormat != AV_PIX_FMT_NONE)
        Reset();

    if (Height < 1 || Width < 1 || Format == AV_PIX_FMT_NONE)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Cannot initialise frame (%1 %2x%3")
            .arg(av_get_pix_fmt_name(Format)).arg(Width).arg(Height));
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Initialise frame: '%1' %2x%3")
        .arg(av_get_pix_fmt_name(Format)).arg(Width).arg(Height));

    AVPixelFormat cpuformat = Format;
    if (Format == AV_PIX_FMT_VDA_VLD)
        m_secondaryPixelFormat = cpuformat = m_preferredDisplayFormat;

    m_rawWidth        = Width;
    m_rawHeight       = Height;
    m_adjustedWidth   = (Width  + 15) & ~0xf;
    m_adjustedHeight  = (Height + 15) & ~0xf;
    m_pixelFormat     = Format;
    m_bitsPerPixel    = av_get_bits_per_pixel(&av_pix_fmt_descriptors[cpuformat]);
    m_numPlanes       = PlaneCount(cpuformat);
    m_bufferSize      = (m_adjustedWidth * m_adjustedHeight * m_bitsPerPixel) >> 3;
}

void VideoFrame::InitialiseBuffer(void)
{
    if (m_buffer)
        return;

    m_buffer = (unsigned char*)av_malloc(m_bufferSize);
    SetOffsets();
}

bool VideoFrame::Discard(void)
{
    return m_discard;
}

void VideoFrame::SetDiscard(void)
{
    m_discard = true;
}

int VideoFrame::PlaneCount(AVPixelFormat Format)
{
    switch (Format)
    {
        case AV_PIX_FMT_YUV420P:
        case AV_PIX_FMT_YUV411P:
        case AV_PIX_FMT_YUV422P:
        case AV_PIX_FMT_YUV444P:
        case AV_PIX_FMT_YUVJ420P:
        case AV_PIX_FMT_YUVJ422P:
        case AV_PIX_FMT_YUVJ444P:
            return 3;
        case AV_PIX_FMT_NV12:
        case AV_PIX_FMT_NV21:
            return 2;
        case AV_PIX_FMT_UYYVYY411:
        case AV_PIX_FMT_UYVY422:
        case AV_PIX_FMT_YUYV422:
        case AV_PIX_FMT_ARGB:
        case AV_PIX_FMT_RGBA:
        case AV_PIX_FMT_ABGR:
        case AV_PIX_FMT_BGRA:
        case AV_PIX_FMT_RGB24:
        case AV_PIX_FMT_BGR24:
        case AV_PIX_FMT_GRAY8:
        case AV_PIX_FMT_BGR8:
        case AV_PIX_FMT_BGR4_BYTE:
        case AV_PIX_FMT_RGB8:
        case AV_PIX_FMT_RGB4_BYTE:
        case AV_PIX_FMT_BGR4:
        case AV_PIX_FMT_RGB4:
        case AV_PIX_FMT_MONOWHITE:
        case AV_PIX_FMT_MONOBLACK:
        case AV_PIX_FMT_GRAY16BE:
        case AV_PIX_FMT_GRAY16LE:
            return 1;
        default:
            return 0; // hardware format or unsupported
    }
}

void VideoFrame::SetOffsets(void)
{
    AVPixelFormat format = m_secondaryPixelFormat != AV_PIX_FMT_NONE ? m_secondaryPixelFormat : m_pixelFormat;

    int halfpitch = m_adjustedWidth >> 1;
    int fullplane = m_adjustedWidth * m_adjustedHeight;

    m_offsets[0] = 0;
    m_pitches[0] = m_adjustedWidth;

    if (m_numPlanes == 1)
    {
        m_offsets[1] = m_offsets[2] = m_offsets[3] = m_offsets[0];
        m_pitches[1] = m_pitches[2] = m_pitches[3] = m_pitches[0];
        return;
    }

    if (m_numPlanes == 2)
    {
        m_offsets[1] = m_offsets[2] = m_offsets[3] = fullplane;
        m_pitches[1] = m_pitches[2] = m_pitches[3] = halfpitch;
        return;
    }

    if (m_numPlanes != 3)
        return;

    if (format == AV_PIX_FMT_YUV420P || format == AV_PIX_FMT_YUVJ420P)
    {
        m_offsets[1] = fullplane;
        m_offsets[2] = m_offsets[3] = m_offsets[1] + (fullplane >> 2);
        m_pitches[3] = m_pitches[2] = m_pitches[1] = halfpitch;
    }
    else if (format == AV_PIX_FMT_YUV411P)
    {
        m_offsets[1] = fullplane;
        m_offsets[2] = m_offsets[3] = m_offsets[1] + (fullplane >> 2);
        m_pitches[1] = m_pitches[2] = m_pitches[3] = halfpitch >> 1;
    }
    else if (format == AV_PIX_FMT_YUV422P || format == AV_PIX_FMT_YUVJ422P)
    {
        m_offsets[1] = fullplane;
        m_offsets[2] = m_offsets[3] = m_offsets[1] + (fullplane >> 1);
        m_pitches[1] = m_pitches[2] = m_pitches[3] = halfpitch;
    }
    else if (format == AV_PIX_FMT_YUV444P || format == AV_PIX_FMT_YUVJ444P)
    {
        m_offsets[1] = fullplane;
        m_offsets[2] = m_offsets[3] = m_offsets[1] + fullplane;
        m_pitches[1] = m_pitches[2] = m_pitches[3] = m_pitches[0];
    }
}
