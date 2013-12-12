/* Class TorcBlurayBuffer
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2013
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
#include "torcblurayhandler.h"
#include "torcbluraybuffer.h"

/*! \class TorcBlurayBuffer
 *  \brief Bluray media playback using libbluray.
 *
 * This is the base implementation. Playback defaults to the main (or longest) title and HDMV and BD-J
 * menus are only supported in TorcBlurayUIBuffer.
 *
 * \sa TorcBlurayUIBuffer
 * \sa TorcBlurayHandler
 * \sa TorcBlurayUIHandler
 *
 * \todo Directory/file check for path. libbluray crashes if the directory doesn't exist.
*/
TorcBlurayBuffer::TorcBlurayBuffer(void *Parent, const QString &URI, int *Abort)
  : TorcBuffer(Parent, URI, Abort),
    m_handler(NULL)
{
}

TorcBlurayBuffer::~TorcBlurayBuffer()
{
     Close();
}

///brief Convert 'bd:/my/path' to '/my/path'
QString TorcBlurayBuffer::GetFilteredUri(void)
{
    return m_uri.mid(3);
}

bool TorcBlurayBuffer::Open(void)
{
    m_state = Status_Opening;

    m_handler = new TorcBlurayHandler(this, GetFilteredUri(), m_abort);
    if (m_handler->Open())
    {
        return true;
    }

    delete m_handler;
    m_handler = NULL;

    m_state = Status_Invalid;
    return false;
}

void TorcBlurayBuffer::Close(void)
{
    delete m_handler;
    m_state = Status_Closed;
}

bool TorcBlurayBuffer::IgnoreEOF(void)
{
    // this may need some additional intelligence...
    return true;
}

int TorcBlurayBuffer::Read(quint8 *Buffer, qint32 BufferSize)
{
    if (m_handler)
        return m_handler->Read(Buffer, BufferSize);

    return -1;
}

int TorcBlurayBuffer::Peek(quint8 *Buffer, qint32 BufferSize)
{
    (void)Buffer;
    (void)BufferSize;
    return 0;
}

int TorcBlurayBuffer::Write(quint8 *Buffer, qint32 BufferSize)
{
    (void)Buffer;
    (void)BufferSize;
    return -1;
}

int64_t TorcBlurayBuffer::Seek(int64_t Offset, int Whence)
{
    (void)Offset;
    (void)Whence;
    return 0;
}

bool TorcBlurayBuffer::IsSequential(void)
{
    return false;
}

qint64 TorcBlurayBuffer::GetSize(void)
{
    return -1;
}

qint64 TorcBlurayBuffer::GetPosition(void)
{
    return 0;
}

qint64 TorcBlurayBuffer::BytesAvailable(void)
{
    // FIXME - this is just a workaround for AudioDecoder::OpenDemuxer
     return 6144LL * 62;
}

int TorcBlurayBuffer::BestBufferSize(void)
{
    return 32768;//6144LL * 62;
}

/*! \class TorcBlurayBufferFactory
 *  \brief A static class to create an bluray disc buffer.
*/
static class TorcBlurayBufferFactory : public TorcBufferFactory
{
    void Score(const QString &URI, const QUrl &URL, int &Score, const bool &Media)
    {
        if (Media && URI.startsWith("bd:", Qt::CaseInsensitive) && Score <= 20)
            Score = 20;
    }

    TorcBuffer* Create(void *Parent, const QString &URI, const QUrl &URL, const int &Score, int *Abort, const bool &Media)
    {
        if (Media && URI.startsWith("bd:", Qt::CaseInsensitive) && Score <= 20)
            return new TorcBlurayBuffer(Parent, URI, Abort);

        return NULL;
    }

} TorcBlurayBufferFactory;
