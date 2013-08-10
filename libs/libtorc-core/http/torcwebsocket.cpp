/* Class TorcWebSocket
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

// Qt
#include <QUrl>
#include <QtEndian>
#include <QCryptographicHash>

// Torc
#include "torclogging.h"
#include "torchttpconnection.h"
#include "torchttprequest.h"
#include "torcwebsocket.h"

/*! \class TorcWebSocket
 *  \brief Overlays the Websocket protocol over a QTcpSocket
 *
 * \sa TorcHTTPServer
 * \sa TorcHTTPRequest
 * \sa TorcHTTPConnection
 *
 * \todo Add protocol handling (JSON, JSON-RPC, XML, XML-RPC, PLIST, PLIST-RPC)
 * \todo Fragmented frames
 * \todo Control frames
 * \todo Interleaved control frames
 * \todo Close handling
 * \todo Client side socket
*/

TorcWebSocket::TorcWebSocket(TorcThread *Parent, TorcHTTPRequest *Request, QTcpSocket *Socket)
  : QObject(),
    m_parent(Parent),
    m_upgradeRequest(Request),
    m_socket(Socket),
    m_abort(0),
    m_serverSide(true),
    m_readState(ReadHeader),
    m_frameFinalFragment(false),
    m_frameOpCode(OpContinuation),
    m_frameMasked(false),
    m_framePayloadLength(0),
    m_frameMask(QByteArray(4, 0)),
    m_framePayload(QByteArray()),
    m_closeCode(CloseNormal)
{
    connect(this, SIGNAL(Close()), this, SLOT(CloseSocket()));
}

TorcWebSocket::~TorcWebSocket()
{
    delete m_upgradeRequest;

    if (m_socket)
    {
        m_socket->disconnect();
        m_socket->close();
        m_socket->deleteLater();
    }

    m_upgradeRequest = NULL;
    m_socket = NULL;
}

///\brief Validate an upgrade request and prepare the appropriate response.
bool TorcWebSocket::ProcessUpgradeRequest(TorcHTTPConnection *Connection, TorcHTTPRequest *Request, QTcpSocket *Socket)
{
    if (!Connection || !Request || !Socket)
        return false;

    bool valid = true;
    bool versionerror = false;
    QString error;

    /* Excerpt from RFC 6455

    The requirements for this handshake are as follows.

       1.   The handshake MUST be a valid HTTP request as specified by
            [RFC2616].

       2.   The method of the request MUST be GET, and the HTTP version MUST
            be at least 1.1.

            For example, if the WebSocket URI is "ws://example.com/chat",
            the first line sent should be "GET /chat HTTP/1.1".
    */

    if (valid && (Request->GetHTTPRequestType() != HTTPGet || Request->GetHTTPProtocol() != HTTPOneDotOne))
    {
        error = "Must be GET and HTTP/1.1";
        valid = false;
    }

    /*
       3.   The "Request-URI" part of the request MUST match the /resource
            name/ defined in Section 3 (a relative URI) or be an absolute
            http/https URI that, when parsed, has a /resource name/, /host/,
            and /port/ that match the corresponding ws/wss URI.
    */

    if (valid && Request->GetPath().isEmpty())
    {
        error = "nvalid Request-URI";
        valid = false;
    }

    /*
       4.   The request MUST contain a |Host| header field whose value
            contains /host/ plus optionally ":" followed by /port/ (when not
            using the default port).
    */

    if (valid && !Request->Headers()->contains("Host"))
    {
        error = "No Host header";
        valid = false;
    }

    // N.B. we should always be using non-default ports, so port must be present
    QUrl host("http://" + Request->Headers()->value("Host"));

    if (valid && host.port() != Connection->GetSocket()->localPort())
    {
        error = "Invalid Host port";
        valid = false;
    }

    if (valid && host.host() != Connection->GetSocket()->localAddress().toString())
    {
        error = "Invalid Host";
        valid = false;
    }

    /*
       5.   The request MUST contain an |Upgrade| header field whose value
            MUST include the "websocket" keyword.
    */

    if (valid && !Request->Headers()->contains("Upgrade"))
    {
        error = "No Upgrade header";
        valid = false;
    }

    if (valid && !Request->Headers()->value("Upgrade").contains("websocket", Qt::CaseInsensitive))
    {
        error = "No 'websocket request";
        valid = false;
    }

    /*
       6.   The request MUST contain a |Connection| header field whose value
            MUST include the "Upgrade" token.
    */

    if (valid && !Request->Headers()->contains("Connection"))
    {
        error = "No Connection header";
        valid = false;
    }

    if (valid && !Request->Headers()->value("Connection").contains("Upgrade", Qt::CaseInsensitive))
    {
        error = "No Upgrade request";
        valid = false;
    }

    /*
       7.   The request MUST include a header field with the name
            |Sec-WebSocket-Key|.  The value of this header field MUST be a
            nonce consisting of a randomly selected 16-byte value that has
            been base64-encoded (see Section 4 of [RFC4648]).  The nonce
            MUST be selected randomly for each connection.

            NOTE: As an example, if the randomly selected value was the
            sequence of bytes 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09
            0x0a 0x0b 0x0c 0x0d 0x0e 0x0f 0x10, the value of the header
            field would be "AQIDBAUGBwgJCgsMDQ4PEC=="
    */

    if (valid && !Request->Headers()->contains("Sec-WebSocket-Key"))
    {
        error = "No Sec-WebSocket-Key header";
        valid = false;
    }

    if (valid && Request->Headers()->value("Sec-WebSocket-Key").isEmpty())
    {
        error = "Invalid Sec-WebSocket-Key";
        valid = false;
    }

    /*
       8.   The request MUST include a header field with the name |Origin|
            [RFC6454] if the request is coming from a browser client.  If
            the connection is from a non-browser client, the request MAY
            include this header field if the semantics of that client match
            the use-case described here for browser clients.  The value of
            this header field is the ASCII serialization of origin of the
            context in which the code establishing the connection is
            running.  See [RFC6454] for the details of how this header field
            value is constructed.

            As an example, if code downloaded from www.example.com attempts
            to establish a connection to ww2.example.com, the value of the
            header field would be "http://www.example.com".

       9.   The request MUST include a header field with the name
            |Sec-WebSocket-Version|.  The value of this header field MUST be
            13.
    */

    if (valid && !Request->Headers()->contains("Sec-WebSocket-Version"))
    {
        error = "No Sec-WebSocket-Version header";
        valid = false;
    }

    int version = Request->Headers()->value("Sec-WebSocket-Version").toInt();

    if (valid && version != Version13)
    {
        error = "Unsupported WebSocket version";
        versionerror = true;
        valid = false;
    }

    /*
        10.  The request MAY include a header field with the name
        |Sec-WebSocket-Protocol|.  If present, this value indicates one
        or more comma-separated subprotocol the client wishes to speak,
        ordered by preference.  The elements that comprise this value
        MUST be non-empty strings with characters in the range U+0021 to
        U+007E not including separator characters as defined in
        [RFC2616] and MUST all be unique strings.  The ABNF for the
        value of this header field is 1#token, where the definitions of
        constructs and rules are as given in [RFC2616].
    */

    if (Request->Headers()->contains("Sec-WebSocket-Protocol"))
    {

    }

    if (!valid)
    {
        Request->SetStatus(HTTP_BadRequest);

        if (versionerror)
            Request->SetResponseHeader("Sec-WebSocket-Version", "13");

        return false;
    }

    // valid handshake so set response headers and transfer socket
    LOG(VB_GENERAL, LOG_INFO, "Received valid websocket Upgrade request");

    QString key = Request->Headers()->value("Sec-WebSocket-Key") + QLatin1String("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    QString nonce = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toBase64();

    Request->SetResponseType(HTTPResponseNone);
    Request->SetStatus(HTTP_SwitchingProtocols);
    Request->SetConnection(HTTPConnectionUpgrade);
    Request->SetResponseHeader("Upgrade", "websocket");
    Request->SetResponseHeader("Sec-WebSocket-Accept", nonce);

    TorcHTTPServer::UpgradeSocket(Request, Socket);

    return true;
}

///\brief Initialise the websocket once its parent thread is ready.
void TorcWebSocket::Start(void)
{
    if (!m_upgradeRequest || !m_socket)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot start Websocket");
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, "WebSocket thread started");

    connect(m_socket, SIGNAL(readyRead()),    this, SLOT(ReadyRead()));
    connect(m_socket, SIGNAL(disconnected()), this, SLOT(Disconnected()));

    m_upgradeRequest->Respond(m_socket, &m_abort);
}

/*! \brief Process incoming data
 *
 * Data for any given frame may be received over a number of packets, hence the need
 * to track the current read status.
*/
void TorcWebSocket::ReadyRead(void)
{
    while (m_socket->bytesAvailable() && !m_abort)
    {
        switch (m_readState)
        {
            case ReadHeader:
            {
                if (m_socket->bytesAvailable() < 2)
                    return;

                char header[2];
                if (m_socket->read(header, 2) != 2)
                {
                    m_closeCode = CloseUnexpectedError;
                    emit Close();
                    return;
                }

                m_frameFinalFragment = (header[0] & 0x80) != 0;
                m_frameOpCode        = static_cast<OpCode>(header[0] & 0x0F);
                m_frameMasked        = (header[1] & 0x80) != 0;
                quint8 length        = (header[1] & 0x7F);

                LOG(VB_NETWORK, LOG_DEBUG, QString("fin %1 op %2 masked %3 length %4").arg(m_frameFinalFragment).arg(m_frameOpCode).arg(m_frameMasked).arg(length));

                // if this is a server, clients must be sending masked frames and vice versa
                if (m_serverSide != m_frameMasked)
                {
                    m_closeCode = CloseProtocolError;
                    emit Close();
                    return;
                }

                if (126 == length)
                {
                    m_readState = Read16BitLength;
                    break;
                }

                if (127 == length)
                {
                    m_readState = Read64BitLength;
                    break;
                }

                m_framePayloadLength = length;
                m_readState          = m_frameMasked ? ReadMask : ReadPayload;
            }
            break;

            case Read16BitLength:
            {
                if (m_socket->bytesAvailable() < 2)
                    return;

                uchar length[2];

                if (m_socket->read(reinterpret_cast<char *>(length), 2) != 2)
                {
                    m_closeCode = CloseUnexpectedError;
                    emit Close();
                    return;
                }

                m_framePayloadLength = qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(length));
                m_readState          = m_frameMasked ? ReadMask : ReadPayload;
            }
            break;

            case Read64BitLength:
            {
                if (m_socket->bytesAvailable() < 8)
                    return;

                uchar length[8];
                if (m_socket->read(reinterpret_cast<char *>(length), 8) != 8)
                {
                    m_closeCode = CloseUnexpectedError;
                    emit Close();
                    return;
                }

                m_framePayloadLength = qFromBigEndian<quint64>(length) & ~(1LL << 63);
                m_readState          = m_frameMasked ? ReadMask : ReadPayload;
            }
            break;

            case ReadMask:
            {
                if (m_socket->bytesAvailable() < 4)
                    return;

                if (m_socket->read(m_frameMask.data(), 4) != 4)
                {
                    m_closeCode = CloseUnexpectedError;
                    emit Close();
                    return;
                }

                m_readState = ReadPayload;
            }
            break;

            case ReadPayload:
            {
                // TODO pre-allocate payload buffer when we know the length
                qint64 available = m_socket->bytesAvailable();
                qint64 needed    = m_framePayloadLength - m_framePayload.size();
                qint64 read      = qMin(available, needed);
                QByteArray data(read, 0);

                if (m_socket->read(data.data(), read) != read)
                {
                    m_closeCode = CloseUnexpectedError;
                    emit Close();
                    return;
                }

                m_framePayload.append(data);

                if (m_framePayload.size() == m_framePayloadLength)
                {
                    LOG(VB_GENERAL, LOG_INFO, "Received complete frame");

                    for (int i = 0; i < m_framePayload.size(); ++i)
                        m_framePayload[i] = (m_framePayload[i] ^ m_frameMask[i % 4]);

                    LOG(VB_GENERAL, LOG_INFO, QString::fromUtf8(m_framePayload));

                    m_readState = ReadHeader;
                    m_framePayload = QByteArray();
                    m_framePayloadLength = 0;
                }
            }
            break;
        }
    }
}

void TorcWebSocket::Disconnected(void)
{
    m_parent->quit();
    LOG(VB_GENERAL, LOG_INFO, "Disconnected");
}

void TorcWebSocket::CloseSocket(void)
{
}

TorcWebSocketThread::TorcWebSocketThread(TorcHTTPRequest *Request, QTcpSocket *Socket)
  : TorcThread("websocket"),
    m_webSocket(new TorcWebSocket(this, Request, Socket))
{
}

TorcWebSocketThread::~TorcWebSocketThread()
{
    m_webSocket->deleteLater();
}

TorcWebSocket* TorcWebSocketThread::Socket(void)
{
    return m_webSocket;
}
