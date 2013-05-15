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
  : TorcBuffer(URI, Abort),
    m_file(NULL)
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

            LOG(VB_GENERAL, LOG_ERR, QString("Failed to open '%1' (%2)")
                .arg(m_uri).arg(m_file->errorString()));
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
    int result = -1;

    if (m_file && m_state == Status_Opened)
    {
        if ((result = m_file->read((char*)Buffer, qMin((qint64)BufferSize, m_file->bytesAvailable()))) > -1)
            return result;

        LOG(VB_GENERAL, LOG_ERR, QString("Read error '%1'").arg(m_file->errorString()));
    }

    return result;
}

int TorcFileBuffer::Peek(quint8 *Buffer, qint32 BufferSize)
{
    int result = -1;

    if (m_file && m_state == Status_Opened)
    {
        if ((result = m_file->peek((char*)Buffer, qMin((qint64)BufferSize, m_file->bytesAvailable()))) > -1)
            return result;

        LOG(VB_GENERAL, LOG_ERR, QString("Peek error '%1'").arg(m_file->errorString()));
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

    if (m_file && m_state == Status_Opened)
    {
        if (AVSEEK_SIZE == whence)
        {
            if (IsSequential())
                return -1;
            return m_file->size();
        }

        QString error = "Uknown request";

        if (SEEK_SET == whence)
        {
            if (m_file->seek(Offset))
                return m_file->pos();

            error = m_file->errorString();
        }

        if (SEEK_CUR == whence)
        {
            if (m_file->seek(m_file->pos() + Offset))
                return m_file->pos();

            error = m_file->errorString();
        }

        if (SEEK_END == whence)
        {
            if (m_file->seek(m_file->size() + Offset))
                return m_file->pos();

            error = m_file->errorString();
        }

        LOG(VB_GENERAL, LOG_ERR, QString("Seek error '%1' (offset: %2 whence %3 file '%4')")
            .arg(error).arg(Offset).arg(whence).arg(m_uri));
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

    if (m_file)
        return m_file->readAll();

    return TorcBuffer::ReadAll();
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
