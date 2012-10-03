/* Class TorcFileBuffer
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
#include "torcfilebuffer.h"

/*! \class TorcFileBuffer
 *  \brief A class for opening local media files.
*/

TorcFileBuffer::TorcFileBuffer(const QString &URI)
  : TorcBuffer(URI),
    m_file(NULL)
{
}

TorcFileBuffer::~TorcFileBuffer()
{
    Close();
}

bool TorcFileBuffer::Open(void)
{
    if (m_file)
    {
        LOG(VB_GENERAL, LOG_WARNING, "File already opened - closing first");
        Close();
    }

    m_state = Status_Opening;
    m_file  = new QFile(m_uri);

    if (m_file)
    {
        if (m_file->exists())
        {
            if (m_file->open(QIODevice::ReadOnly))
            {
                m_state = Status_Opened;
                return TorcBuffer::Open();
            }

            LOG(VB_GENERAL, LOG_ERR, QString("Faled to open '%1'").arg(m_uri));
        }

        LOG(VB_GENERAL, LOG_ERR, QString("File '%1' does not exist").arg(m_uri));

        delete m_file;
        m_file = NULL;
    }

    m_state = Status_Invalid;
    return false;
}

void TorcFileBuffer::Close(void)
{
    TorcBuffer::Close();

    m_state = Status_Closing;

    if (m_file)
    {
        m_file->close();
        m_file->deleteLater();
    }

    m_file = NULL;

    m_state = Status_Closed;
}

int TorcFileBuffer::Read(quint8 *Buffer, qint32 BufferSize)
{
    if (m_file && m_state == Status_Opened)
        return m_file->read((char*)Buffer, BufferSize);
    return -1;
}

int TorcFileBuffer::Peek(quint8 *Buffer, qint32 BufferSize)
{
    if (m_file && m_state == Status_Opened)
        return m_file->peek((char*)Buffer, BufferSize);
    return -1;
}

int TorcFileBuffer::Write(quint8 *Buffer, qint32 BufferSize)
{
    (void)Buffer;
    (void)BufferSize;
    return -1;
}

qint64 TorcFileBuffer::Seek(qint64 Offset, int Whence)
{
    int whence = Whence & ~AVSEEK_FORCE;

    if (m_file && m_state == Status_Opened)
    {
        if (AVSEEK_SIZE == whence)
        {
            if (IsSequential())
                return -1;
            return m_file->size();
        }

        if (SEEK_SET == whence && m_file->seek(Offset))
            return m_file->pos();

        if (SEEK_CUR == whence && m_file->seek(m_file->pos() + Offset))
            return m_file->pos();

        if (SEEK_END == whence && m_file->seek(m_file->size() - Offset))
            return m_file->pos();

        LOG(VB_GENERAL, LOG_ERR, QString("Seek error offset: %1 whence %2 file '%3'")
            .arg(Offset).arg(whence).arg(m_uri));

        return -1;
    }

    return -1;
}

qint64 TorcFileBuffer::GetSize(void)
{
    if (m_file)
        return m_file->size();
    return -1;
}

qint64 TorcFileBuffer::GetPosition(void)
{
    if (m_file)
        return m_file->pos();
    return -1;
}

bool TorcFileBuffer::IsSequential(void)
{
    if (m_file)
        return m_file->isSequential();
    return true;
}

qint64 TorcFileBuffer::BytesAvailable(void)
{
    if (m_file)
        return m_file->bytesAvailable();
    return 0;
}

static class TorcFileBufferFactory : public TorcBufferFactory
{
    TorcBuffer* Create(const QString &URI, const QUrl &URL)
    {
        if (URL.isRelative() && URL.port() < 0)
        {
            TorcFileBuffer* result = new TorcFileBuffer(URI);
            if (result->Open())
                return result;

            delete result;
        }

        return NULL;
    }

} TorcFileBufferFactory;
