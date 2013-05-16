/* Class TorcFileBuffer
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

//Qt
#include <QDir>
#include <QFileInfo>

// Torc
#include "torclogging.h"
#include "torcfilebuffer.h"

/*! \class TorcFileBuffer
 *  \brief A class for opening local media files.
*/

TorcFileBuffer::TorcFileBuffer(const QString &URI, int *Abort)
  : QFile(URI),
    TorcBuffer(URI, Abort)
{
    QFileInfo info(m_uri);
    m_path = info.dir().path();
}

TorcFileBuffer::~TorcFileBuffer()
{
    Close();
}

bool TorcFileBuffer::Open(void)
{
    m_state = Status_Opening;

    if (exists())
    {
        if (open(QIODevice::ReadOnly))
        {
            m_state = Status_Opened;
            return TorcBuffer::Open();
        }

        LOG(VB_GENERAL, LOG_ERR, QString("Failed to open '%1' (%2)")
            .arg(m_uri).arg(errorString()));
    }

    LOG(VB_GENERAL, LOG_ERR, QString("File '%1' does not exist").arg(m_uri));

    m_state = Status_Invalid;
    return false;
}

void TorcFileBuffer::Close(void)
{
    TorcBuffer::Close();

    m_state = Status_Closing;

    close();

    m_state = Status_Closed;
}

int TorcFileBuffer::Read(quint8 *Buffer, qint32 BufferSize)
{
    int result = -1;

    if (m_state == Status_Opened)
    {
        if ((result = read((char*)Buffer, qMin((qint64)BufferSize, bytesAvailable()))) > -1)
            return result;

        LOG(VB_GENERAL, LOG_ERR, QString("Read error '%1'").arg(errorString()));
    }

    return result;
}

int TorcFileBuffer::Peek(quint8 *Buffer, qint32 BufferSize)
{
    int result = -1;

    if (m_state == Status_Opened)
    {
        if ((result = peek((char*)Buffer, qMin((qint64)BufferSize, bytesAvailable()))) > -1)
            return result;

        LOG(VB_GENERAL, LOG_ERR, QString("Peek error '%1'").arg(errorString()));
    }

    return result;
}

int TorcFileBuffer::Write(quint8 *Buffer, qint32 BufferSize)
{
    (void)Buffer;
    (void)BufferSize;
    return -1;
}

int64_t TorcFileBuffer::Seek(int64_t Offset, int Whence)
{
    int whence = Whence & ~AVSEEK_FORCE;

    if (m_state == Status_Opened && !IsSequential())
    {
        if (AVSEEK_SIZE == whence)
        {
            return size();
        }
        else if (SEEK_SET == whence || SEEK_CUR == whence || SEEK_END == whence)
        {
            qint64 newoffset = Offset; // SEEK_SET

            if (SEEK_CUR == whence)
                newoffset = pos() + Offset;
            else if (SEEK_END == whence)
                newoffset = size() + Offset;

            if (seek(Offset))
                return pos();

            LOG(VB_GENERAL, LOG_ERR, QString("Seek error '%1' (offset: %2 whence %3 file '%4')")
                .arg(errorString()).arg(Offset).arg(whence).arg(m_uri));
        }
    }

    return -1;
}

QString TorcFileBuffer::GetPath(void)
{
    return m_path;
}

QByteArray TorcFileBuffer::ReadAll(int Timeout)
{
    (void)Timeout;

    return readAll();
}

qint64 TorcFileBuffer::GetSize(void)
{
    return size();
}

qint64 TorcFileBuffer::GetPosition(void)
{
    return pos();
}

bool TorcFileBuffer::IsSequential(void)
{
    return isSequential();
}

qint64 TorcFileBuffer::BytesAvailable(void)
{
    return bytesAvailable();
}

int TorcFileBuffer::BestBufferSize(void)
{
    return 32768;
}

static class TorcFileBufferFactory : public TorcBufferFactory
{
    void Score(const QString &URI, const QUrl &URL, int &Score, const bool &Media)
    {
        if ((URL.scheme().size() < 2) && URL.port() < 0 && Score <= 10)
            Score = 10;
    }

    TorcBuffer* Create(const QString &URI, const QUrl &URL, const int &Score, int *Abort, const bool &Media)
    {
        if ((URL.scheme().size() < 2) && URL.port() < 0 && Score <= 10)
            return new TorcFileBuffer(URI, Abort);

        return NULL;
    }

} TorcFileBufferFactory;
