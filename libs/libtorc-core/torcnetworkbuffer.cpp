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

TorcNetworkBuffer::TorcNetworkBuffer(const QString &URI, bool Streamed, int *Abort)
  : TorcBuffer(URI, Abort),
    m_streamed(Streamed),
    m_request(NULL)
{
}

TorcNetworkBuffer::~TorcNetworkBuffer()
{
    Close();
}

bool TorcNetworkBuffer::Open(void)
{
    if (!TorcNetwork::IsAvailable())
        return false;

    QUrl url(m_uri);
    if (!url.isValid())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Invalid URL '%1'").arg(m_uri));
        return false;
    }

    if (m_request)
    {
        LOG(VB_GENERAL, LOG_INFO, "Connection already open - closing before re-opening");
        Close();
    }

    QFileInfo info(url.path());
    m_path = url.scheme() + "://" + url.authority() + info.path();

    m_state = Status_Opening;

    QNetworkRequest request(url);
    m_request = new TorcNetworkRequest(request, m_streamed ? DEFAULT_STREAMED_BUFFER_SIZE : 0, m_abort);
    if (TorcNetwork::Get(m_request))
    {
        m_request->SetReadSize(DEFAULT_STREAMED_READ_SIZE);
        m_state = Status_Opened;
        return TorcBuffer::Open();
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

    if (m_state == Status_Opened && m_request && m_streamed)
        if ((result = m_request->Read((char*)Buffer, BufferSize, 20000)) > -1)
            return result;

    if (!m_streamed)
        LOG(VB_GENERAL, LOG_ERR, "Trying to read from a non-streamed download");

    return result;
}

int TorcNetworkBuffer::Peek(quint8 *Buffer, qint32 BufferSize)
{
    (void)Buffer;
    (void)BufferSize;
    return -1;
}

int64_t TorcNetworkBuffer::Seek(int64_t Offset, int Whence)
{
    (void)Offset;
    (void)Whence;
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
    return -1;
}

qint64 TorcNetworkBuffer::GetPosition(void)
{
    return -1;
}

bool TorcNetworkBuffer::IsSequential(void)
{
    return true;
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
            if (TorcNetwork::IsAvailable())
                Score = 50;
            else
                LOG(VB_GENERAL, LOG_WARNING, "Network access not available");
        }
    }

    TorcBuffer* Create(const QString &URI, const QUrl &URL, const int &Score, int *Abort, const bool &Media)
    {
        if ((QString::compare(URL.scheme(), "http",  Qt::CaseInsensitive) == 0 ||
             QString::compare(URL.scheme(), "https", Qt::CaseInsensitive) == 0 ||
             QString::compare(URL.scheme(), "ftp",   Qt::CaseInsensitive) == 0) &&
            Score <= 50 && TorcNetwork::IsAvailable())
        {
            return new TorcNetworkBuffer(URI, Media, Abort);
        }

        return NULL;
    }
} TorcNetworkBufferFactory;
