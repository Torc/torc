/* Class TorcNetworkBuffer
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
#include "torccoreutils.h"
#include "torctimer.h"
#include "torcnetworkbuffer.h"

TorcNetworkBuffer::TorcNetworkBuffer(const QString &URI, bool Media, int *Abort)
  : TorcBuffer(URI, Abort),
    m_media(Media),
    m_type(Media ? Buffered : Unbuffered),
    m_request(NULL)
{
}

TorcNetworkBuffer::~TorcNetworkBuffer()
{
    Close();
}

bool TorcNetworkBuffer::Open(void)
{
    // sanity checks
    if (!TorcNetwork::IsAllowedOutbound())
        return false;

    QUrl url(m_uri);
    if (!url.isValid())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Invalid URL '%1'").arg(m_uri));
        return false;
    }

    // close existing connections just in case
    if (m_request)
    {
        LOG(VB_GENERAL, LOG_INFO, "Connection already open - closing before re-opening");
        Close();
    }

    // determine our path (for theme root etc)
    QFileInfo info(url.path());
    m_path = url.scheme() + "://" + url.authority() + info.path();

    // and finally try and open
    m_state = Status_Opening;
    int buffersize = DEFAULT_STREAMED_BUFFER_SIZE;

    // can we use byte serving/streamed download for media files
    if (m_media && m_type == Buffered && QString::compare(url.scheme(), "ftp", Qt::CaseInsensitive) != 0)
    {
        QNetworkRequest request(url);
        TorcNetworkRequest *test = new TorcNetworkRequest(request, QNetworkAccessManager::HeadOperation,
                                                          0, m_abort);
        if (TorcNetwork::Get(test))
        {
            test->ReadAll(NETWORK_TIMEOUT);
            if (test->CanByteServe())
            {
                LOG(VB_GENERAL, LOG_INFO, QString("Streaming available for '%1'").arg(url.toString()));

                m_type = Streamed;

                if (test->GetFinalURL() != url)
                {
                    url = test->GetFinalURL();
                    LOG(VB_GENERAL, LOG_INFO, QString("Updating url to '%1").arg(url.toString()));
                }

                // restrict the buffer size for small files
                if (test->GetSize() > 0)
                    buffersize = qMax(qint64(1024 * 1024), qMin((qint64)buffersize, test->GetSize()));
            }

            TorcNetwork::Cancel(test);
        }

        test->DownRef();
    }

    QNetworkRequest request(url);
    m_request = new TorcNetworkRequest(request, QNetworkAccessManager::GetOperation,
                                       m_type == Unbuffered ? 0 : buffersize, m_abort);
    m_request->SetReadSize(DEFAULT_STREAMED_READ_SIZE);

    if (TorcNetwork::Get(m_request))
    {
        if (m_request->WaitForStart(NETWORK_TIMEOUT))
        {
            LOG(VB_NETWORK, LOG_INFO, QString("Content length: %1 Type: %2").arg(m_request->GetSize()).arg(m_request->GetContentType()));
            m_state = Status_Opened;
            return TorcBuffer::Open();
        }

        LOG(VB_GENERAL, LOG_ERR, "Waited too long for dowload to start");
        TorcNetwork::Cancel(m_request);
        m_request->DownRef();
        m_request = NULL;
    }

    m_state = Status_Invalid;
    return false;
}

void TorcNetworkBuffer::Close(void)
{
    TorcBuffer::Close();

    m_state = Status_Closing;

    if (m_request)
    {
        TorcNetwork::Cancel(m_request);
        m_request->DownRef();
    }
    m_request = NULL;

    m_state = Status_Closed;
}

int TorcNetworkBuffer::Read(quint8 *Buffer, qint32 BufferSize)
{
    int result = -1;

    if (m_state == Status_Opened && m_request && m_type != Unbuffered)
        if ((result = m_request->Read((char*)Buffer, BufferSize, NETWORK_TIMEOUT)) > -1)
            return result;

    if (m_type == Unbuffered)
        LOG(VB_GENERAL, LOG_ERR, "Trying to read from a non-streamed download");

    return result;
}

int TorcNetworkBuffer::Peek(quint8 *Buffer, qint32 BufferSize)
{
    if (m_state == Status_Opened && m_request && m_type != Unbuffered)
        return m_request->Peek((char*)Buffer, BufferSize, NETWORK_TIMEOUT);

    return -1;
}

int64_t TorcNetworkBuffer::Seek(int64_t Offset, int Whence)
{
    if (m_type != Streamed)
        return -1;

    int whence = Whence & ~AVSEEK_FORCE;

    if (m_request && m_state == Status_Opened )
    {
        if (AVSEEK_SIZE == whence)
        {
            return m_request->GetSize();
        }
        else if (SEEK_SET == whence || SEEK_CUR == whence || SEEK_END == whence)
        {
            qint64 newoffset = Offset; // SEEK_SET

            if (SEEK_CUR == whence)
                newoffset = m_request->GetPosition() + Offset;
            else if (SEEK_END == whence)
                newoffset = m_request->GetSize() + Offset;

            if (m_request->Seek(newoffset) > -1)
                return m_request->GetPosition();

            QNetworkRequest request(m_request->GetFinalURL());
            int buffersize = qMax(qint64(1024 * 1024), qMin((qint64)DEFAULT_STREAMED_BUFFER_SIZE, m_request->GetSize() - newoffset));
            TorcNetworkRequest *seek = new TorcNetworkRequest(request, QNetworkAccessManager::GetOperation, buffersize, m_abort);
            seek->SetRange(newoffset);

            if (TorcNetwork::Get(seek))
            {
                if (seek->WaitForStart(NETWORK_TIMEOUT))
                {
                    TorcNetwork::Cancel(m_request);
                    m_request->DownRef();
                    m_request = seek;
                    return m_request->GetPosition();
                }

                TorcNetwork::Cancel(seek);
            }

            LOG(VB_GENERAL, LOG_INFO, "Seek failed");
            seek->DownRef();
        }
    }

    return -1;
}

QByteArray TorcNetworkBuffer::ReadAll(int Timeout)
{
    if (m_request)
        return m_request->ReadAll(Timeout);

    return QByteArray();
}

int TorcNetworkBuffer::Write(quint8 *Buffer, qint32 BufferSize)
{
    (void)Buffer;
    (void)BufferSize;
    return -1;
}

qint64 TorcNetworkBuffer::GetSize(void)
{
    if (m_request)
        m_request->GetSize();
    return -1;
}

qint64 TorcNetworkBuffer::GetPosition(void)
{
    if (m_request)
        m_request->GetPosition();
    return -1;
}

bool TorcNetworkBuffer::IsSequential(void)
{
    return m_type != Streamed;
}

qint64 TorcNetworkBuffer::BytesAvailable(void)
{
    if (m_request)
        return m_request->BytesAvailable();
    return -1;
}

int TorcNetworkBuffer::BestBufferSize(void)
{
    return DEFAULT_STREAMED_READ_SIZE;
}

QString TorcNetworkBuffer::GetPath(void)
{
    return m_path;
}

class TorcNetworkBufferFactory : public TorcBufferFactory
{
    void Score(const QString &URI, const QUrl &URL, int &Score, const bool &Media)
    {
        if ((QString::compare(URL.scheme(), "http",  Qt::CaseInsensitive) == 0 ||
             QString::compare(URL.scheme(), "https", Qt::CaseInsensitive) == 0 ||
             QString::compare(URL.scheme(), "ftp",   Qt::CaseInsensitive) == 0) &&
            Score <= 50)
        {
            if (TorcNetwork::IsAllowedOutbound())
                Score = 50;
            else
                LOG(VB_GENERAL, LOG_WARNING, "Outbound network access not available");
        }
    }

    TorcBuffer* Create(const QString &URI, const QUrl &URL, const int &Score, int *Abort, const bool &Media)
    {
        if ((QString::compare(URL.scheme(), "http",  Qt::CaseInsensitive) == 0 ||
             QString::compare(URL.scheme(), "https", Qt::CaseInsensitive) == 0 ||
             QString::compare(URL.scheme(), "ftp",   Qt::CaseInsensitive) == 0) &&
            Score <= 50 && TorcNetwork::IsAllowedOutbound())
        {
            return new TorcNetworkBuffer(URI, Media, Abort);
        }

        return NULL;
    }
} TorcNetworkBufferFactory;
