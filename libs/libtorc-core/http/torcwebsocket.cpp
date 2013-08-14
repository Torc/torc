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

// utf8
#include "utf8/core.h"
#include "utf8/checked.h"
#include "utf8/unchecked.h"

/*! \class TorcWebSocket
 *  \brief Overlays the Websocket protocol over a QTcpSocket
 *
 * \sa TorcHTTPServer
 * \sa TorcHTTPRequest
 * \sa TorcHTTPConnection
 *
 * \note To test using the Autobahn python test suite, configure the suite to
 *       request a connection using 'echo' as the method (e.g. 'http://your-ip-address:your-port/echo').
 *
 * \todo Add protocol handling (JSON, JSON-RPC, XML, XML-RPC, PLIST, PLIST-RPC)
 * \todo Complete close handshake (rather than just closing)
 * \todo Fix thread exiting
 * \todo Client side socket
 * \todo Limit frame size for reading
 * \todo Fix testsuite partial failures (fail fast on invalid UTF-8)
*/

TorcWebSocket::TorcWebSocket(TorcThread *Parent, TorcHTTPRequest *Request, QTcpSocket *Socket)
  : QObject(),
    m_parent(Parent),
    m_upgradeRequest(Request),
    m_socket(Socket),
    m_abort(0),
    m_serverSide(true),
    m_readState(ReadHeader),
    m_echoTest(false),
    m_frameFinalFragment(false),
    m_frameOpCode(OpContinuation),
    m_frameMasked(false),
    m_framePayloadLength(0),
    m_frameMask(QByteArray(4, 0)),
    m_framePayload(QByteArray()),
    m_framePayloadReadPosition(0),
    m_bufferedPayload(NULL),
    m_bufferedPayloadOpCode(OpContinuation),
    m_closeCode(CloseNormal),
    m_closeReceived(false),
    m_closeSent(false)
{
    if (Request->GetMethod().startsWith(QStringLiteral("echo"), Qt::CaseInsensitive))
    {
        m_echoTest = true;
        LOG(VB_GENERAL, LOG_INFO, "Enabling WebSocket echo for testing");
    }

    connect(this, SIGNAL(Close()), this, SLOT(CloseSocket()));
}

TorcWebSocket::~TorcWebSocket()
{
    if (m_socket)
    {
        m_socket->disconnect();
        m_socket->close();
        m_socket->deleteLater();
    }

    delete m_upgradeRequest;
    delete m_bufferedPayload;

    m_upgradeRequest  = NULL;
    m_bufferedPayload = NULL;
    m_socket          = NULL;

    LOG(VB_GENERAL, LOG_INFO, "WebSocket dtor");
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

    // disable host check. It offers us no additional security and may be a raw
    // ip address, domain name or or other host name.
#if 0
    if (valid && host.host() != Connection->GetSocket()->localAddress().toString())
    {
        error = "Invalid Host";
        valid = false;
    }
#endif
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
        LOG(VB_GENERAL, LOG_ERR, error);

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

///\brief Convert OpCode to human readable string
QString TorcWebSocket::OpCodeToString(OpCode Code)
{
    switch (Code)
    {
        case OpContinuation: return QString("Continuation");
        case OpText:         return QString("Text");
        case OpBinary:       return QString("Binary");
        case OpClose:        return QString("Close");
        case OpPing:         return QString("Ping");
        case OpPong:         return QString("Pong");
        default: break;
    }

    return QString("Reserved");
}

///\brief Convert CloseCode to human readable string
QString TorcWebSocket::CloseCodeToString(CloseCode Code)
{
    switch (Code)
    {
        case CloseNormal:              return QString("Normal");
        case CloseGoingAway:           return QString("GoingAway");
        case CloseProtocolError:       return QString("ProtocolError");
        case CloseUnsupportedDataType: return QString("UnsupportedDataType");
        case CloseReserved1004:        return QString("Reserved");
        case CloseStatusCodeMissing:   return QString("StatusCodeMissing");
        case CloseAbnormal:            return QString("Abnormal");
        case CloseInconsistentData:    return QString("InconsistentData");
        case ClosePolicyViolation:     return QString("PolicyViolation");
        case CloseMessageTooBig:       return QString("MessageTooBig");
        case CloseMissingExtension:    return QString("MissingExtension");
        case CloseUnexpectedError:     return QString("UnexpectedError");
        case CloseTLSHandshakeError:   return QString("TLSHandshakeError");
        default: break;
    }

    return QString("Unknown");
}

///\brief Initialise the websocket once its parent thread is ready.
void TorcWebSocket::Start(void)
{
    if (!m_upgradeRequest || !m_socket)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot start Websocket");
        if (m_parent)
            m_parent->quit();
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
    while ((m_socket->bytesAvailable() || (m_readState == ReadPayload && m_framePayloadLength == 0)) && !m_abort)
    {
        switch (m_readState)
        {
            case ReadHeader:
            {
                // we need at least 2 bytes to start reading
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
                bool reservedbits    = (header[0] & 0x70) != 0;

                // validate the header against current state and specification
                CloseCode error = CloseNormal;

                // invalid use of reserved bits
                if (reservedbits)
                {
                    m_closeReason = QString("Invalid use of reserved bits");
                    error = CloseProtocolError;
                }

                // control frames can only have payloads of up to 125 bytes
                else if ((m_frameOpCode & 0x8) && length > 125)
                {
                    m_closeReason = QString("Control frame payload too large");
                    error = CloseProtocolError;
                }

                // if this is a server, clients must be sending masked frames and vice versa
                else if (m_serverSide != m_frameMasked)
                {
                    m_closeReason = QString("Masking error");
                    error = CloseProtocolError;
                }

                // we should never receive a reserved opcode
                else if (m_frameOpCode != OpText && m_frameOpCode != OpBinary && m_frameOpCode != OpContinuation &&
                    m_frameOpCode != OpPing && m_frameOpCode != OpPong   && m_frameOpCode != OpClose)
                {
                    m_closeReason = QString("Invalid use of reserved opcode");
                    error = CloseProtocolError;
                }

                // control frames cannot be fragmented
                else if (!m_frameFinalFragment && (m_frameOpCode == OpPing || m_frameOpCode == OpPong || m_frameOpCode == OpClose))
                {
                    m_closeReason = QString("Fragmented control frame");
                    error = CloseProtocolError;
                }

                // a continuation frame must have an opening frame
                else if (!m_bufferedPayload && m_frameOpCode == OpContinuation)
                {
                    m_closeReason = QString("Fragmentation error");
                    error = CloseProtocolError;
                }

                // only continuation frames or control frames are expected once in the middle of a frame
                else if (m_bufferedPayload && !(m_frameOpCode == OpContinuation || m_frameOpCode == OpPing || m_frameOpCode == OpPong || m_frameOpCode == OpClose))
                {
                    m_closeReason = QString("Fragmentation error");
                    error = CloseProtocolError;
                }

                if (error != CloseNormal)
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("Read error: %1 (%2)").arg(CloseCodeToString(error)).arg(m_closeReason));
                    m_closeCode = error;
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
                // allocate the payload buffer if needed
                if (m_framePayloadReadPosition == 0)
                    m_framePayload = QByteArray(m_framePayloadLength, 0);

                qint64 needed = m_framePayloadLength - m_framePayloadReadPosition;

                // payload length may be zero
                if (needed > 0)
                {
                    qint64 read = qMin(m_socket->bytesAvailable(), needed);

                    if (m_socket->read(m_framePayload.data() + m_framePayloadReadPosition, read) != read)
                    {
                        m_closeCode = CloseUnexpectedError;
                        emit Close();
                        return;
                    }

                    m_framePayloadReadPosition += read;
                }

                // finished
                if (m_framePayloadReadPosition == m_framePayloadLength)
                {
                    LOG(VB_NETWORK, LOG_DEBUG, QString("Frame, Final: %1 OpCode: '%2' Masked: %3 Length: %4")
                        .arg(m_frameFinalFragment).arg(OpCodeToString(m_frameOpCode)).arg(m_frameMasked).arg(m_framePayloadLength));

                    // unmask payload
                    if (m_frameMasked)
                        for (int i = 0; i < m_framePayload.size(); ++i)
                            m_framePayload[i] = (m_framePayload[i] ^ m_frameMask[i % 4]);

                    // start buffering fragmented payloads
                    if (!m_frameFinalFragment && (m_frameOpCode == OpText || m_frameOpCode == OpBinary))
                    {
                        // header checks should prevent this
                        if (m_bufferedPayload)
                        {
                            delete m_bufferedPayload;
                            LOG(VB_GENERAL, LOG_WARNING, "Already have payload buffer - deleting");
                        }

                        m_bufferedPayload = new QByteArray(m_framePayload);
                        m_bufferedPayloadOpCode = m_frameOpCode;
                    }
                    else if (m_frameOpCode == OpContinuation)
                    {
                        if (m_bufferedPayload)
                            m_bufferedPayload->append(m_framePayload);
                        else
                            LOG(VB_GENERAL, LOG_ERR, "Continuation frame but no buffered payload");
                    }

                    // finished
                    if (m_frameFinalFragment)
                    {
                        if (m_frameOpCode == OpPong)
                        {
                            HandlePong(m_framePayload);
                        }
                        else if (m_frameOpCode == OpPing)
                        {
                            HandlePing(m_framePayload);
                        }
                        else if (m_frameOpCode == OpClose)
                        {
                            HandleClose(m_framePayload);
                        }
                        else
                        {
                            bool invalidtext = false;

                            // validate and debug UTF8 text
                            if (!m_bufferedPayload && m_frameOpCode == OpText && !m_framePayload.isEmpty())
                            {
                                if (!utf8::is_valid(m_framePayload.data(), m_framePayload.data() + m_framePayload.size()))
                                {
                                    LOG(VB_GENERAL, LOG_ERR, "Invalid UTF8");
                                    invalidtext = true;
                                }
                                else
                                {
                                    LOG(VB_NETWORK, LOG_DEBUG, QString("'%1'").arg(QString::fromUtf8(m_framePayload)));
                                }
                            }
                            else if (m_bufferedPayload && m_bufferedPayloadOpCode == OpText)
                            {
                                if (!utf8::is_valid(m_bufferedPayload->data(), m_bufferedPayload->data() + m_bufferedPayload->size()))
                                {
                                    LOG(VB_GENERAL, LOG_ERR, "Invalid UTF8");
                                    invalidtext = true;
                                }
                                else
                                {
                                    LOG(VB_NETWORK, LOG_DEBUG, QString("'%1'").arg(QString::fromUtf8(*m_bufferedPayload)));
                                }
                            }

                            if (invalidtext)
                            {
                                m_closeCode = CloseInconsistentData;
                                emit Close();
                            }
                            else
                            {
                                // echo test for AutoBahn test suite
                                if (m_echoTest)
                                {
                                    SendFrame(m_bufferedPayload ? m_bufferedPayloadOpCode : m_frameOpCode,
                                              m_bufferedPayload ? *m_bufferedPayload : m_framePayload);
                                }

                                // TODO actually do something with real data
                            }
                            delete m_bufferedPayload;
                            m_bufferedPayload = NULL;
                        }
                    }

                    // reset frame and readstate
                    m_readState = ReadHeader;
                    m_framePayload = QByteArray();
                    m_framePayloadReadPosition = 0;
                    m_framePayloadLength = 0;
                }
                else if (m_framePayload.size() > m_framePayloadLength)
                {
                    // this shouldn't happen
                    LOG(VB_GENERAL, LOG_ERR, "Buffer overflow");
                    m_closeCode = CloseUnexpectedError;
                    emit Close();
                    return;
                }
            }
            break;
        }
    }
}

void TorcWebSocket::Disconnected(void)
{
    if (m_parent)
        m_parent->quit();

    LOG(VB_GENERAL, LOG_INFO, "Disconnected");
}

void TorcWebSocket::CloseSocket(void)
{
    m_socket->disconnect();
    m_socket->close();

    // FIXME - disconnect above isn't triggering...
    Disconnected();
}

/*! \brief Compose and send a properly formatted websocket frame.
 *
 * \note If this is a client side socket, Payload will be masked in place.
*/
void TorcWebSocket::SendFrame(OpCode Code, QByteArray &Payload)
{
    QByteArray frame;
    QByteArray mask;
    QByteArray size;

    // no fragmentation yet - so this is always the final fragment
    frame.append(Code | 0x80);

    quint8 byte = 0;

    // is this masked
    if (!m_serverSide)
    {
        for (int i = 0; i < 4; ++i)
            mask.append(qrand() % 0x100);

        byte |= 0x80;
    }

    // generate correct size
    quint64 length = Payload.size();

    if (length < 126)
    {
        byte |= length;
    }
    else if (length <= 0xffff)
    {
        byte |= 126;
        size.append((length >> 8) & 0xff);
        size.append(length & 0xff);
    }
    else if (length <= 0x7fffffff)
    {
        byte |= 127;
        size.append((length >> 56) & 0xff);
        size.append((length >> 48) & 0xff);
        size.append((length >> 40) & 0xff);
        size.append((length >> 32) & 0xff);
        size.append((length >> 24) & 0xff);
        size.append((length >> 16) & 0xff);
        size.append((length >> 8 ) & 0xff);
        size.append((length      ) & 0xff);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Infeasibly large payload!");
        return;
    }

    frame.append(byte);
    if (size.size())
        frame.append(size);

    if (!m_serverSide)
    {
        frame.append(mask);
        for (int i = 0; i < Payload.size(); ++i)
            Payload[i] = Payload[i] ^ mask[i % 4];
    }

    if (m_socket->write(frame) == frame.size())
    {
        if ((Payload.size() && m_socket->write(Payload) == Payload.size()) || !Payload.size())
        {
            m_socket->flush();
            LOG(VB_NETWORK, LOG_DEBUG, QString("Sent frame (Final), OpCode: '%1' Masked: %2 Length: %3")
                .arg(OpCodeToString(Code)).arg(!m_serverSide).arg(Payload.size()));
            return;
        }
    }

    LOG(VB_GENERAL, LOG_ERR, "Error sending frame");
    m_closeCode = CloseUnexpectedError;
    emit Close();
}

void TorcWebSocket::HandlePing(QByteArray &Payload)
{
    if (m_closeReceived || m_closeSent)
        return;

    SendFrame(OpPong, Payload);
}

void TorcWebSocket::HandlePong(QByteArray &Payload)
{
    // TODO validate known pings
}

void TorcWebSocket::HandleClose(QByteArray &Close)
{
    CloseCode closecode = CloseNormal;
    QString reason;

    // check close code if present
    if (Close.size() > 1)
    {
        closecode = (CloseCode)qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(Close.data()));
        if ("Unknown" == CloseCodeToString(closecode) || closecode == CloseReserved1004 ||
            closecode == CloseStatusCodeMissing || closecode == CloseAbnormal || closecode == CloseTLSHandshakeError)
        {
            if (!(closecode >= 3000 && closecode <= 4999))
            {
                LOG(VB_GENERAL, LOG_ERR, "Invalid close code");
                // change code
                Close[0] = 0x03;
                Close[1] = 0xEA;
            }
        }
    }

    // check close reason if present
    if (Close.size() > 2)
    {
        if (!utf8::is_valid(Close.data() + 2, Close.data() + Close.size()))
        {
            LOG(VB_GENERAL, LOG_ERR, "Invalid UTF8 in close payload");
            // change close code
            Close[0] = 0x03;
            Close[1] = 0xEF;
        }
        else
        {
            reason = QString::fromUtf8(Close.data() + 2, Close.size() -2);
        }
    }

    LOG(VB_NETWORK, LOG_INFO, QString("Received Close: %1 ('%2')").arg(CloseCodeToString(closecode)).arg(reason));
    m_closeReceived = true;

    if (m_closeSent)
    {
        CloseSocket();
    }
    else
    {
        // echo back the payload and close request
        SendFrame(OpClose, Close);
        m_closeSent = true;
        CloseSocket();
    }
}

TorcWebSocketThread::TorcWebSocketThread(TorcHTTPRequest *Request, QTcpSocket *Socket)
  : TorcThread("websocket"),
    m_webSocket(new TorcWebSocket(this, Request, Socket))
{
}

TorcWebSocketThread::~TorcWebSocketThread()
{
    m_webSocket->deleteLater();
    LOG(VB_GENERAL, LOG_INFO, "WebSocketThread dtor");
}

TorcWebSocket* TorcWebSocketThread::Socket(void)
{
    return m_webSocket;
}
