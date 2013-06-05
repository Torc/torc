/* Class TorcNetworkRequest
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
#include "torcadminthread.h"
#include "http/torchttprequest.h"
#include "torccoreutils.h"
#include "torcnetworkrequest.h"

#include <errno.h>

/*! \class TorcNetworkRequest
 *  \brief A wrapper around QNetworkRequest
 *
 * TorcNetworkRequest acts as the intermediary between TorcNetwork and any other
 * class accessing the network.
 *
 * The requestor must create a valid QNetworkRequest, an abort signal and, if a streamed
 * download is required, provide a buffer size.
 *
 * In the case of streamed downloads, a circular buffer of the given size is created which
 * must be emptied (via Read) on a frequent basis. The readBufferSize for the associated QNetworkReply
 * is set to the same size. Hence there is matched buffering between Qt and Torc. Streamed downloads
 * are optimised for a single producer and a single consumer thread - any other usage is likely to lead
 * to tears before bedtime.
 *
 * For non-streamed requests, the complete download is copied to the internal buffer and can be
 * retrieved via ReadAll. Take care when downloading files of an unknown size.
 *
 * \todo Complete streamed support for seeking and peeking
 * \todo Add user messaging for network errors
*/

TorcNetworkRequest::TorcNetworkRequest(const QNetworkRequest Request, QNetworkAccessManager::Operation Type, int BufferSize, int *Abort)
  : m_type(Type),
    m_abort(Abort),
    m_started(false),
    m_positionInFile(0),
    m_rewindPositionInFile(0),
    m_readPosition(0),
    m_writePosition(0),
    m_bufferSize(BufferSize),
    m_reserveBufferSize(BufferSize >> 3),
    m_writeBufferSize(BufferSize - m_reserveBufferSize),
    m_buffer(QByteArray(BufferSize, 0)),
    m_readSize(DEFAULT_STREAMED_READ_SIZE),
    m_redirectionCount(0),
    m_readTimer(new TorcTimer()),
    m_writeTimer(new TorcTimer()),
    m_replyFinished(false),
    m_replyBytesAvailable(0),
    m_bytesReceived(0),
    m_bytesTotal(0),
    m_request(Request),
    m_rangeStart(0),
    m_rangeEnd(0),
    m_httpStatus(HTTP_BadRequest),
    m_contentLength(0),
    m_byteServingAvailable(false)
{
    // reserve some space for seeking backwards in the stream
    if (m_bufferSize)
        LOG(VB_GENERAL, LOG_INFO, QString("Request buffer size %1bytes (%2 reserved)").arg(m_bufferSize).arg(m_bufferSize - m_writeBufferSize));
}

TorcNetworkRequest::~TorcNetworkRequest()
{
    delete m_readTimer;
    delete m_writeTimer;
    m_readTimer = NULL;
    m_writeTimer = NULL;
}

int TorcNetworkRequest::BytesAvailable(void)
{
    if (!m_bufferSize)
        return m_buffer.size();

    return m_availableToRead.fetchAndAddOrdered(0);
}

qint64 TorcNetworkRequest::GetSize(void)
{
    return m_contentLength;
}

qint64 TorcNetworkRequest::GetPosition(void)
{
    if (!m_bufferSize)
        return -1;

    return m_positionInFile;
}

bool TorcNetworkRequest::WaitForStart(int Timeout)
{
    m_readTimer->Restart();

    while (!m_started && !m_replyFinished && !(*m_abort))
        TorcUSleep(50000);

    return m_started || m_replyFinished;
}

int TorcNetworkRequest::Peek(char *Buffer, qint32 BufferSize, int Timeout)
{
    return Read(Buffer, BufferSize, Timeout, true);
}

/*! \fn Seek
 * If this is a streamed download, attempt to seek within the range
 * of what is currently buffered.
*/
qint64 TorcNetworkRequest::Seek(qint64 Offset)
{
    // unbuffered
    if (!m_bufferSize)
        return -1;

    // beyond currently downloaded
    // TODO wait if within a 'reasonable' distance
    if (Offset > (m_positionInFile + BytesAvailable()))
        return -1;

    // seek backwards
    if (Offset < m_positionInFile)
        if (Offset < m_rewindPositionInFile)
            return -1;

    // seek within the currently available buffer
    int seek = Offset - m_positionInFile;
    m_readPosition += seek;
    if (m_readPosition >= m_bufferSize)
        m_readPosition -= m_bufferSize;
    else if (m_readPosition < 0)
        m_readPosition += m_bufferSize;

    m_positionInFile = Offset;
    m_availableToRead.fetchAndAddOrdered(-seek);

    return Offset;
}

int TorcNetworkRequest::Read(char *Buffer, qint32 BufferSize, int Timeout, bool Peek)
{
    if (!Buffer || !m_bufferSize)
        return -1;

    m_readTimer->Restart();

    while ((BytesAvailable() < m_readSize) && !(*m_abort) && (m_readTimer->Elapsed() < Timeout) && !(m_replyFinished && m_replyBytesAvailable == 0))
    {
        if (m_replyBytesAvailable && m_writeTimer->Elapsed() > 100)
            gNetwork->Poke(this);
        TorcUSleep(50000);
    }

    if (*m_abort)
        return -ECONNABORTED;

    int available = BytesAvailable();
    if (available < 1)
    {
        if (m_replyFinished && m_replyBytesAvailable == 0 && available == 0)
        {
            LOG(VB_GENERAL, LOG_INFO, "Download complete");
            return 0;
        }

        LOG(VB_GENERAL, LOG_ERR, QString("Waited %1ms for data. Aborting").arg(Timeout));
        return -ECONNABORTED;
    }

    available = qMin(available, BufferSize);

    if (m_readPosition + available > m_bufferSize)
    {
        int rest = m_bufferSize - m_readPosition;
        memcpy(Buffer, m_buffer.data() + m_readPosition, rest);
        memcpy(Buffer + rest, m_buffer.data(), available - rest);

        if (!Peek)
            m_readPosition = available - rest;
    }
    else
    {
        memcpy(Buffer, m_buffer.data() + m_readPosition, available);
        if (!Peek)
            m_readPosition += available;
    }

    if (!Peek)
    {
        if (m_readPosition >= m_bufferSize)
            m_readPosition -= m_bufferSize;

        m_positionInFile += available;

        m_rewindPositionInFile = qMax(m_rewindPositionInFile, m_positionInFile < m_reserveBufferSize ? 0 : m_positionInFile - m_reserveBufferSize);

        m_availableToRead.fetchAndAddOrdered(-available);
    }

    return available;
}

bool TorcNetworkRequest::WritePriv(QNetworkReply *Reply, char *Buffer, int Size)
{
    if (Reply && Buffer && Size)
    {
        int read = Reply->read(Buffer, Size);

        if (read < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Error reading '%1'").arg(Reply->errorString()));
            return false;
        }

        m_writePosition += read;
        if (m_writePosition >= m_bufferSize)
            m_writePosition -= m_bufferSize;
        m_availableToRead.fetchAndAddOrdered(read);

        m_replyBytesAvailable = Reply->bytesAvailable();

        if (read == Size)
            return true;

        LOG(VB_GENERAL, LOG_ERR, QString("Expected %1 bytes, got %2").arg(Size).arg(read));
    }

    return false;
}

void TorcNetworkRequest::Write(QNetworkReply *Reply)
{
    if (!Reply)
        return;

    m_writeTimer->Restart();

    if (!m_bufferSize)
    {
        m_buffer += Reply->readAll();
        return;
    }

    qint64 available = qMin(m_writeBufferSize - (qint64)m_availableToRead.fetchAndAddOrdered(0), Reply->bytesAvailable());

    if (m_writePosition + available > m_bufferSize)
    {
        int rest = m_bufferSize - m_writePosition;

        if (!WritePriv(Reply, m_buffer.data() + m_writePosition, rest))
            return;
        if (!WritePriv(Reply, m_buffer.data(), available - rest))
            return;
    }
    else
    {
        WritePriv(Reply, m_buffer.data() + m_writePosition, available);
    }
}

void TorcNetworkRequest::SetReadSize(int Size)
{
    m_readSize = Size;
}

void TorcNetworkRequest::SetRange(int Start, int End)
{
    if (m_rangeStart != 0 || m_rangeEnd != 0)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Byte ranges already set - ignoring");
        return;
    }

    if (Start < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, "Invalid range start");
        return;
    }

    m_rangeStart = Start;
    m_rangeEnd   = End;
    if (m_rangeEnd <= Start)
        m_rangeEnd = -1;

    // adjust known position in file
    m_positionInFile       += m_rangeStart;
    m_rewindPositionInFile += m_rangeStart;

    // and update the request
    m_request.setRawHeader("Range", QString("bytes=%1-%2").arg(m_rangeStart).arg(m_rangeEnd != -1 ? QString::number(m_rangeEnd) : "").toLatin1());
}

void TorcNetworkRequest::DownloadProgress(qint64 Received, qint64 Total)
{
    m_bytesReceived = Received;
    m_bytesTotal    = Total;
}

bool TorcNetworkRequest::CanByteServe(void)
{
    return m_byteServingAvailable;
}

QUrl TorcNetworkRequest::GetFinalURL(void)
{
    return m_request.url();
}

QString TorcNetworkRequest::GetContentType(void)
{
    return m_contentType;
}

QByteArray TorcNetworkRequest::ReadAll(int Timeout)
{
    if (m_bufferSize)
        LOG(VB_GENERAL, LOG_WARNING, "ReadAll called for a streamed buffer");

    m_readTimer->Restart();

    while (!(*m_abort) && (m_readTimer->Elapsed() < Timeout) && !m_replyFinished)
        TorcUSleep(50000);

    if (*m_abort)
        return QByteArray();

    if (!m_replyFinished)
        LOG(VB_GENERAL, LOG_ERR, "Download timed out - probably incomplete");

    return m_buffer;
}
