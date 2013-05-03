/* Class TorcBuffer
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
#include "torcbuffer.h"

/*! \class TorcBuffer
 *  \brief The base class for opening media files and streams.
*/

TorcBuffer::TorcBuffer(const QString &URI)
  : m_uri(URI),
    m_state(Status_Created),
    m_paused(true),
    m_bitrate(0),
    m_bitrateFactor(1)
{
}

TorcBuffer::~TorcBuffer()
{
}

TorcBuffer* TorcBuffer::Create(const QString &URI, bool Media)
{
    TorcBuffer* buffer = NULL;
    QUrl url(URI);

    int score = 0;
    TorcBufferFactory* factory = TorcBufferFactory::GetTorcBufferFactory();
    for ( ; factory; factory = factory->NextTorcBufferFactory())
        (void)factory->Score(URI, url, score, Media);

    factory = TorcBufferFactory::GetTorcBufferFactory();
    for ( ; factory; factory = factory->NextTorcBufferFactory())
    {
        buffer = factory->Create(URI, url, score, Media);
        if (buffer)
            break;
    }

    if (!buffer)
        LOG(VB_GENERAL, LOG_WARNING, QString("Failed to create buffer for '%1'").arg(URI));

    return buffer;
}

void* TorcBuffer::RequiredAVContext(void)
{
    return NULL;
}

void* TorcBuffer::RequiredAVFormat(void)
{
    return NULL;
}

int TorcBuffer::Read(void *Object, quint8 *Buffer, qint32 BufferSize)
{
    TorcBuffer* buffer = static_cast<TorcBuffer*>(Object);

    if (buffer)
        return buffer->Read(Buffer, BufferSize);

    return -1;
}

int TorcBuffer::Write(void *Object, quint8 *Buffer, qint32 BufferSize)
{
    TorcBuffer* buffer = static_cast<TorcBuffer*>(Object);

    if (buffer)
        return buffer->Write(Buffer, BufferSize);

    return -1;
}

int64_t TorcBuffer::Seek(void *Object, int64_t Offset, int Whence)
{
    TorcBuffer* buffer = static_cast<TorcBuffer*>(Object);

    if (buffer)
        return buffer->Seek(Offset, Whence);

    return -1;
}

int (*TorcBuffer::GetReadFunction(void))(void*, quint8*, qint32)
{
    return &TorcBuffer::Read;
}

int (*TorcBuffer::GetWriteFunction(void))(void*, quint8*, qint32)
{
    return &TorcBuffer::Write;
}

int64_t (*TorcBuffer::GetSeekFunction(void))(void*, int64_t, int)
{
    return &TorcBuffer::Seek;
}

bool TorcBuffer::Open(void)
{
    LOG(VB_GENERAL, LOG_INFO, QString("Opened '%1'").arg(m_uri));
    return Unpause();
}

void TorcBuffer::Close(void)
{
    LOG(VB_GENERAL, LOG_INFO, QString("Closing '%1'").arg(m_uri));
}

bool TorcBuffer::HandleAction(int Action)
{
    return false;
}

QByteArray TorcBuffer::ReadAll(void)
{
    return QByteArray();
}

bool TorcBuffer::Pause(void)
{
    if (m_paused)
        return false;

    m_paused = true;
    m_state  = Status_Paused;

    return true;
}


bool TorcBuffer::Unpause(void)
{
    if (!m_paused)
        return false;

    m_paused = false;
    m_state  = Status_Opened;
    return true;
}

bool TorcBuffer::TogglePause(void)
{
    m_paused = !m_paused;
    m_state = m_paused ? Status_Paused : Status_Opened;
    return m_paused;
}

bool TorcBuffer::GetPaused(void)
{
    return m_paused;
}

void TorcBuffer::SetBitrate(int Bitrate, int Factor)
{
    m_bitrate = Bitrate;
    m_bitrateFactor = Factor;

    LOG(VB_GENERAL, LOG_INFO, QString("New bitrate: %1 kbit/s (factor %2)")
        .arg(m_bitrate / 1000).arg(m_bitrateFactor));
}

QString TorcBuffer::GetFilteredUri(void)
{
    return m_uri;
}

QString TorcBuffer::GetFilteredPath(void)
{
    return m_uri;
}

QString TorcBuffer::GetURI(void)
{
    return m_uri;
}

TorcBufferFactory* TorcBufferFactory::gTorcBufferFactory = NULL;

TorcBufferFactory::TorcBufferFactory()
{
    m_nextTorcBufferFactory = gTorcBufferFactory;
    gTorcBufferFactory = this;
}

TorcBufferFactory::~TorcBufferFactory()
{
}

TorcBufferFactory* TorcBufferFactory::GetTorcBufferFactory(void)
{
    return gTorcBufferFactory;
}

TorcBufferFactory* TorcBufferFactory::NextTorcBufferFactory(void) const
{
    return m_nextTorcBufferFactory;
}
