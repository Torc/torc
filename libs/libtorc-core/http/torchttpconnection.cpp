/* Class TorcHTTPConnection
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
#include "torchttprequest.h"
#include "torchttpconnection.h"

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
 *
 * \todo Handle abort in TorcHTTRequest::Respond
 * \todo Check content length early and deal with large or unexpected content payloads
 * \todo Return media and static content (i.e. handle files)
*/

TorcHTTPConnection::TorcHTTPConnection(TorcHTTPServer *Parent, qintptr SocketDescriptor, int *Abort)
  : QRunnable(),
    m_abort(Abort),
    m_server(Parent),
    m_socketDescriptor(SocketDescriptor),
    m_socket(NULL)
{
}

TorcHTTPConnection::~TorcHTTPConnection()
{
}

QTcpSocket* TorcHTTPConnection::GetSocket(void)
{
    return m_socket;
}

void TorcHTTPConnection::run(void)
{
    if (!m_socketDescriptor)
        return;

    // create socket
    m_socket = new QTcpSocket();
    if (!m_socket->setSocketDescriptor(m_socketDescriptor))
    {
        LOG(VB_GENERAL, LOG_INFO, "Failed to set socket descriptor");
        return;
    }

    // debug
    QString peeraddress  = m_socket->peerAddress().toString() + ":" + QString::number(m_socket->peerPort());
    QString localaddress = m_socket->localAddress().toString() + ":" + QString::number(m_socket->localPort());

    LOG(VB_NETWORK, LOG_INFO, "New connection from" + peeraddress + " on " + localaddress);

    // iniitialise state
    bool requeststarted     = false;
    bool headerscomplete    = false;
    int  headersread        = 0;
    quint64 contentlength   = 0;
    quint64 contentreceived = 0;
    QString method          = QString();
    QByteArray *content     = new QByteArray();
    QMap<QString,QString> *headers = new QMap<QString,QString>();

    while (!(*m_abort) && m_socket->state() == QAbstractSocket::ConnectedState)
    {
        // wait for data
        int count = 0;
        while (m_socket->state() == QAbstractSocket::ConnectedState && !(*m_abort) &&
               count++ < 300 && m_socket->bytesAvailable() < 1 && !m_socket->waitForReadyRead(100))
        {
        }

        // timed out
        if (count >= 300)
        {
            LOG(VB_NETWORK, LOG_INFO, "No socket activity for 30 seconds");
            break;
        }

        if (*m_abort || m_socket->state() != QAbstractSocket::ConnectedState)
            break;

        // sanity check
        if (headersread >= 200)
        {
            LOG(VB_GENERAL, LOG_ERR, "Read 200 lines of headers - aborting");
            break;
        }

        // read headers
        if (!headerscomplete)
        {
            while (!(*m_abort) && m_socket->canReadLine() && headersread < 200)
            {
                QByteArray line = m_socket->readLine().trimmed();

                if (line.isEmpty())
                {
                    headersread = 0;
                    headerscomplete = true;
                    break;
                }

                if (!requeststarted)
                {
                    LOG(VB_NETWORK, LOG_DEBUG, line);
                    method = line;
                }
                else
                {
                    int index = line.indexOf(":");

                    if (index > 0)
                    {
                        QByteArray key   = line.left(index).trimmed();
                        QByteArray value = line.mid(index + 1).trimmed();

                        if (key == "Content-Length")
                            contentlength = value.toULongLong();

                        LOG(VB_NETWORK, LOG_DEBUG, QString("%1: %2").arg(key.data()).arg(value.data()));

                        headers->insert(key, value);
                    }
                }

                requeststarted = true;
            }
        }

        if (*m_abort || m_socket->state() != QAbstractSocket::ConnectedState || !headerscomplete)
            continue;

        // read content
        while (!(*m_abort) && (contentreceived < contentlength) && m_socket->bytesAvailable() &&
               m_socket->state() == QAbstractSocket::ConnectedState)
        {
            static quint64 MAX_CHUNK = 32 * 1024;
            content->append(m_socket->read(qMax(contentlength - contentreceived, qMax(MAX_CHUNK, (quint64)m_socket->bytesAvailable()))));
            contentreceived = content->size();
        }

        if (*m_abort || m_socket->state() != QAbstractSocket::ConnectedState || contentreceived < contentlength)
            continue;

        // sanity check
        if (m_socket->bytesAvailable() > 0)
            LOG(VB_GENERAL, LOG_WARNING, QString("%1 unread bytes from %2").arg(m_socket->bytesAvailable()).arg(peeraddress));

        // have headers and content - process request
        TorcHTTPRequest *request = new TorcHTTPRequest(method, headers, content);

        if (request->GetHTTPType() == HTTPResponse)
        {
            LOG(VB_GENERAL, LOG_ERR, "Received unexpected HTTP response");
        }
        else
        {
            m_server->HandleRequest(this, request);
            if (m_socket)
                request->Respond(m_socket);
        }

        // this will delete content and headers
        delete request;

        // reset
        requeststarted  = false;
        headerscomplete = false;
        contentlength   = 0;
        contentreceived = 0;
        method          = QString();
        content         = new QByteArray();
        headers         = new QMap<QString,QString>();
    }

    // cleanup
    delete headers;
    delete content;

    if (*m_abort)
        LOG(VB_NETWORK, LOG_INFO, "Socket closed at server's request");

    if (m_socket->state() != QAbstractSocket::ConnectedState)
        LOG(VB_NETWORK, LOG_INFO, "Socket was disconnected by remote host");

    m_socket->disconnectFromHost();
    delete m_socket;
    m_socket = NULL;

    LOG(VB_NETWORK, LOG_INFO, QString("Connection from %1 closed").arg(peeraddress));
}
