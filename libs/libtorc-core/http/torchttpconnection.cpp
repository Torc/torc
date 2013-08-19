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

/*! \class TorcHTTPReader
 *  \brief A convenience class to read HTTP requests from a QTcpSocket
 *
 * \note Both m_content and m_headers MAY be transferred to new parents for processing. Handle with care.
*/
TorcHTTPReader::TorcHTTPReader()
  : m_ready(false),
    m_requestStarted(false),
    m_headersComplete(false),
    m_headersRead(0),
    m_contentLength(0),
    m_contentReceived(0),
    m_method(QString()),
    m_content(new QByteArray()),
    m_headers(new QMap<QString,QString>())
{
}

TorcHTTPReader::~TorcHTTPReader()
{
    Reset();
}

///\brief Reset the read state
void TorcHTTPReader::Reset(void)
{
    delete m_content;
    delete m_headers;

    m_ready           = false;
    m_requestStarted  = false;
    m_headersComplete = false;
    m_headersRead     = 0;
    m_contentLength   = 0;
    m_contentReceived = 0;
    m_method          = QString();
    m_content         = new QByteArray();
    m_headers         = new QMap<QString,QString>();
}

/*! \brief Read and parse data from the given socket.
 *
 * The HTTP method (e.g. GET / 200 OK), headers and content will be split out
 * for further processing.
 */
bool TorcHTTPReader::Read(QTcpSocket *Socket, int *Abort)
{
    if (!Socket)
        return false;

    if (*Abort || Socket->state() != QAbstractSocket::ConnectedState)
        return false;

    // sanity check
    if (m_headersRead >= 200)
    {
        LOG(VB_GENERAL, LOG_ERR, "Read 200 lines of headers - aborting");
        return false;
    }

    // read headers
    if (!m_headersComplete)
    {
        while (!(*Abort) && Socket->canReadLine() && m_headersRead < 200)
        {
            QByteArray line = Socket->readLine().trimmed();

            if (line.isEmpty())
            {
                m_headersRead = 0;
                m_headersComplete = true;
                break;
            }

            if (!m_requestStarted)
            {
                LOG(VB_NETWORK, LOG_DEBUG, line);
                m_method = line;
            }
            else
            {
                m_headersRead++;
                int index = line.indexOf(":");

                if (index > 0)
                {
                    QByteArray key   = line.left(index).trimmed();
                    QByteArray value = line.mid(index + 1).trimmed();

                    if (key == "Content-Length")
                        m_contentLength = value.toULongLong();

                    LOG(VB_NETWORK, LOG_DEBUG, QString("%1: %2").arg(key.data()).arg(value.data()));

                    m_headers->insert(key, value);
                }
            }

            m_requestStarted = true;
        }
    }

    // loop if we need more header data
    if (!m_headersComplete)
        return true;

    // abort early if needed
    if (*Abort || Socket->state() != QAbstractSocket::ConnectedState)
        return false;

    // read content
    while (!(*Abort) && (m_contentReceived < m_contentLength) && Socket->bytesAvailable() &&
           Socket->state() == QAbstractSocket::ConnectedState)
    {
        static quint64 MAX_CHUNK = 32 * 1024;
        m_content->append(Socket->read(qMax(m_contentLength - m_contentReceived, qMax(MAX_CHUNK, (quint64)Socket->bytesAvailable()))));
        m_contentReceived = m_content->size();
    }

    // loop if we need more data
    if (m_contentReceived < m_contentLength)
        return true;

    // abort early if needed
    if (*Abort || Socket->state() != QAbstractSocket::ConnectedState)
        return false;

    m_ready = true;
    return true;
}

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
 * \todo Check content length early and deal with large or unexpected content payloads
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

    LOG(VB_NETWORK, LOG_INFO, "New connection from " + peeraddress + " on " + localaddress);

    // iniitialise state
    TorcHTTPReader *reader  = new TorcHTTPReader();
    bool connectionupgraded = false;

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

        // read data
        if (!reader->Read(m_socket, m_abort))
            break;

        if (!reader->m_ready)
            continue;

        // sanity check
        if (m_socket->bytesAvailable() > 0)
            LOG(VB_GENERAL, LOG_WARNING, QString("%1 unread bytes from %2").arg(m_socket->bytesAvailable()).arg(peeraddress));

        // have headers and content - process request
        TorcHTTPRequest *request = new TorcHTTPRequest(reader->m_method, reader->m_headers, reader->m_content);

        // request will take ownerhsip of headers and content
        reader->m_headers = NULL;
        reader->m_content = NULL;

        if (request->GetHTTPType() == HTTPResponse)
        {
            LOG(VB_GENERAL, LOG_ERR, "Received unexpected HTTP response");
        }
        else if (request->Headers()->contains("Upgrade"))
        {
            // if the connection is upgraded, both request and m_socket will be transferred
            // to a new thread. DO NOT DELETE!
            if (TorcWebSocket::ProcessUpgradeRequest(this, request, m_socket))
            {
                connectionupgraded = true;
                break;
            }
            else
            {
                request->Respond(m_socket, m_abort);
            }
        }
        else
        {
            m_server->HandleRequest(this, request);
            if (m_socket)
                request->Respond(m_socket, m_abort);
        }

        // this will delete content and headers
        delete request;

        // reset
        reader->Reset();
    }

    // cleanup
    delete reader;

    if (!connectionupgraded)
    {
        if (*m_abort)
            LOG(VB_NETWORK, LOG_INFO, "Socket closed at server's request");

        if (m_socket->state() != QAbstractSocket::ConnectedState)
            LOG(VB_NETWORK, LOG_INFO, "Socket was disconnected by remote host");

        m_socket->disconnectFromHost();
        delete m_socket;
        m_socket = NULL;

        LOG(VB_NETWORK, LOG_INFO, QString("Connection from %1 closed").arg(peeraddress));
    }
    else
    {
        LOG(VB_NETWORK, LOG_INFO, "Exiting - connection upgrading to full thread");
    }
}
