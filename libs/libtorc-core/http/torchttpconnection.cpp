/* Class TorcHTTPConnection
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
#include "torchttprequest.h"
#include "torchttpserver.h"
#include "torchttpconnection.h"

// is this too large?
#define BUFFER_ERROR_SIZE (128 * 1024)

/*! \class TorcHTTPConnection
 *  \brief A handler for an HTTP client connection.
 *
 * TorcHTTPConnection encapsulates a current TCP connection from an HTTP client.
 * It processes incoming data and prepares complete request data to be passed
 * to TorcHTTPRequest. When a complete and valid request has been processed, it
 * is passed back to the parent TorcHTTPServer for processing.
 *
 * \sa TorcHTTPServer
 * \sa TorcHTTPHandler
 * \sa TorcHTTPRequest
*/

TorcHTTPConnection::TorcHTTPConnection(TorcHTTPServer *Parent, QTcpSocket *Socket)
  : QObject(),
    m_server(Parent),
    m_socket(Socket)
{
    m_buffer.open(QIODevice::ReadWrite);

    Reset();

    connect(m_socket,  SIGNAL(readyRead()),     this,     SLOT(ReadFromClient()));
    connect(&m_buffer, SIGNAL(readyRead()),     this,     SLOT(ReadInternal()));
    connect(m_socket,  SIGNAL(disconnected()),  m_server, SLOT(ClientDisconnected()));
    connect(this,      SIGNAL(NewRequest()),    m_server, SLOT(NewRequest()));
    m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
}

TorcHTTPConnection::~TorcHTTPConnection()
{
    m_buffer.close();

    delete m_headers;
    delete m_content;

    Reset();

    foreach (TorcHTTPRequest* request, m_requests)
        delete request;

    if (m_socket)
    {
        m_socket->disconnectFromHost();
        m_socket->deleteLater();
    }

    m_socket = NULL;
}

void TorcHTTPConnection::Reset(void)
{
    m_requestStarted = false;
    m_headersComplete = false;
    m_contentLength = 0;
    m_contentReceived = 0;
    m_headers = new QMap<QString,QString>();
    m_content = new QByteArray();
}

void TorcHTTPConnection::ReadFromClient(void)
{
    qint64 position = m_buffer.pos();
    while (m_socket && m_socket->bytesAvailable())
        m_buffer.write(m_socket->readAll());
    m_buffer.seek(position);

    // if the buffer is getting large, we've messed up parsing, so reset and try to recover
    if (m_buffer.bytesAvailable() > BUFFER_ERROR_SIZE)
    {
        LOG(VB_GENERAL, LOG_ERR, "HTTP buffer size exceeded - resetting connection");
        m_buffer.readAll();
        delete m_content;
        delete m_headers;
        Reset();
    }
}

void TorcHTTPConnection::ReadInternal(void)
{
    while (m_buffer.bytesAvailable())
    {
        if (!m_headersComplete)
        {
            while (m_buffer.canReadLine())
            {
                QByteArray line = m_buffer.readLine().trimmed();

                if (line.isEmpty())
                {
                    m_headersComplete = true;
                    break;
                }

                ProcessHeader(line, m_requestStarted);
                m_requestStarted = true;
            }
        }

        if (m_headersComplete && (m_contentReceived < m_contentLength) && m_buffer.bytesAvailable())
        {
            m_content->append(m_buffer.read(m_contentLength - m_contentReceived));
            m_contentReceived = m_content->size();
        }

        if (m_headersComplete && (m_contentReceived == m_contentLength))
        {
            m_requests.append(new TorcHTTPRequest(m_method, m_headers, m_content));
            Reset();
            emit NewRequest();
        }

        if ((!m_headersComplete && !m_buffer.canReadLine()) ||
            (m_headersComplete && !m_buffer.bytesAvailable()))
        {
            break;
        }
    }
}

void TorcHTTPConnection::ProcessHeader(const QByteArray &Line, bool Started)
{
    if (!Started)
    {
        m_method = Line;
        return;
    }

    int index = Line.indexOf(":");

    if (index < 1)
        return;

    QByteArray key   = Line.left(index).trimmed();
    QByteArray value = Line.mid(index + 1).trimmed();

    if (key == "Content-Length")
        m_contentLength = value.toULongLong();

    m_headers->insert(key, value);
}

bool TorcHTTPConnection::HasRequests(void)
{
    return !m_requests.isEmpty();
}

TorcHTTPRequest* TorcHTTPConnection::GetRequest(void)
{
    if (m_requests.isEmpty())
        return NULL;

    return m_requests.takeFirst();
}

void TorcHTTPConnection::Complete(TorcHTTPRequest *Request)
{
    if (Request && m_socket)
    {
        QPair<QByteArray*,QByteArray*> response = Request->Respond();
        QByteArray* headers = response.first;
        QByteArray* content = response.second;

        if (headers)
        {
            qint64 size = headers->size();
            qint64 sent = m_socket->write(headers->data(), size);
            if (size != sent)
                LOG(VB_GENERAL, LOG_WARNING, QString("Bufer size %1 - but sent %2").arg(size).arg(sent));
        }

        if (content)
        {
            qint64 size = content->size();
            qint64 sent = m_socket->write(content->data(), size);
            if (size != sent)
                LOG(VB_GENERAL, LOG_WARNING, QString("Bufer size %1 - but sent %2").arg(size).arg(sent));
        }

        m_socket->flush();

        if (!Request->KeepAlive())
            m_socket->disconnectFromHost();
    }

    delete Request;
}
