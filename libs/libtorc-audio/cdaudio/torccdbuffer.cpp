/* Class TorcCDBuffer
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
#include "torccdbuffer.h"

/*! \class TorcCDBuffer
 *  \brief A simple TorcBuffer wrapper that creates the correct libcdio based input context for audio CD playback.
 *
 * \todo Replace the FFmpeg implementation with an internal version that handles all metadata lookups etc.
 *
 * \sa TorcBuffer
*/
TorcCDBuffer::TorcCDBuffer(const QString &URI, int *Abort)
  : TorcBuffer(URI, Abort),
    m_input(NULL)
{
}

TorcCDBuffer::~TorcCDBuffer()
{
}

void* TorcCDBuffer::RequiredAVFormat(void)
{
    return m_input;
}

void TorcCDBuffer::InitialiseAVContext(void* Context)
{
    // with FFmpeg 1.2, cd playback fails as the dts is not set. This affects ffplay as well.
    AVFormatContext* context = static_cast<AVFormatContext*>(Context);
    if (context && context->nb_streams > 0)
        context->streams[0]->cur_dts = 0;
}

QString TorcCDBuffer::GetFilteredUri(void)
{
    return m_uri.mid(3);
}

bool TorcCDBuffer::Open(void)
{
    m_input = av_find_input_format("libcdio");

    if (!m_input)
        LOG(VB_GENERAL, LOG_ERR, "libcdio demuxer not found");

    return m_input != NULL;
}

void TorcCDBuffer::Close(void)
{
}

int TorcCDBuffer::Read(quint8 *Buffer, qint32 BufferSize)
{
    (void)Buffer;
    (void)BufferSize;
    return 0;
}

int TorcCDBuffer::Peek(quint8 *Buffer, qint32 BufferSize)
{
    (void)Buffer;
    (void)BufferSize;
    return 0;
}

int TorcCDBuffer::Write(quint8 *Buffer, qint32 BufferSize)
{
    (void)Buffer;
    (void)BufferSize;
    return -1;
}

int64_t TorcCDBuffer::Seek(int64_t Offset, int Whence)
{
    (void)Offset;
    (void)Whence;
    return 0;
}

qint64 TorcCDBuffer::GetSize(void)
{
    return -1;
}

qint64 TorcCDBuffer::GetPosition(void)
{
    return 0;
}

bool TorcCDBuffer::IsSequential(void)
{
    return false;
}

qint64 TorcCDBuffer::BytesAvailable(void)
{
    return 0;
}

int TorcCDBuffer::BestBufferSize(void)
{
    return 0;
}

/*! \class TorcCDBufferFactory
 *  \brief A static class to create an audio CD input buffer.
*/
static class TorcCDBufferFactory : public TorcBufferFactory
{
    void Score(const QString &URI, const QUrl &URL, int &Score, const bool &Media)
    {
        if (Media && URI.startsWith("cd:", Qt::CaseInsensitive) && Score <= 20)
            Score = 20;
    }

    TorcBuffer* Create(const QString &URI, const QUrl &URL, const int &Score, int *Abort, const bool &Media)
    {
        if (Media && URI.startsWith("cd:", Qt::CaseInsensitive) && Score <= 20)
            return new TorcCDBuffer(URI, Abort);

        return NULL;
    }

} TorcCDBufferFactory;
