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
#include <QTimer>
#include <QtEndian>
#include <QTextStream>
#include <QJsonDocument>
#include <QCryptographicHash>

// Torc
#include "torclogging.h"
#include "torcnetwork.h"
#include "torcnetworkedcontext.h"
#include "torchttpconnection.h"
#include "torchttprequest.h"
#include "torcrpcrequest.h"
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
 * \note SubProtocol support is currently limited to JSON-RPC. New subprotocols with mixed frame support
 *       (Binary and Text) are not currently supported.
 *
 * \note To test using the Autobahn python test suite, configure the suite to
 *       request a connection using 'echo' as the method (e.g. 'http://your-ip-address:your-port/echo').
 *
 * \todo Limit frame size for reading
 * \todo Fix testsuite partial failures (fail fast on invalid UTF-8)
 * \todo Add timeout for response to upgrade request
 * \todo Fix deletion handling when there is no parent thread
 * \todo Add support for batched RPC calls
*/

TorcWebSocket::TorcWebSocket(TorcQThread *Parent, TorcHTTPRequest *Request, QTcpSocket *Socket)
  : QObject(),
    m_parent(Parent),
    m_authenticate(false),
    m_handShaking(false),
    m_upgradeResponseReader(NULL),
    m_address(QHostAddress()),
    m_port(0),
    m_upgradeRequest(Request),
    m_socket(Socket),
    m_abort(0),
    m_serverSide(true),
    m_readState(ReadHeader),
    m_echoTest(false),
    m_subProtocol(SubProtocolNone),
    m_subProtocolFrameFormat(OpText),
    m_frameFinalFragment(false),
    m_frameOpCode(OpContinuation),
    m_frameMasked(false),
    m_framePayloadLength(0),
    m_frameMask(QByteArray(4, 0)),
    m_framePayload(QByteArray()),
    m_framePayloadReadPosition(0),
    m_bufferedPayload(NULL),
    m_bufferedPayloadOpCode(OpContinuation),
    m_closeReceived(false),
    m_closeSent(false),
    m_currentRequestID(1)
{
    if (Request->GetMethod().startsWith(QStringLiteral("echo"), Qt::CaseInsensitive))
    {
        m_echoTest = true;
        LOG(VB_GENERAL, LOG_INFO, "Enabling WebSocket echo for testing");
    }

    if (Request->Headers()->contains("Sec-WebSocket-Protocol"))
    {
        QList<WSSubProtocol> protocols = SubProtocolsFromPrioritisedString(Request->Headers()->value("Sec-WebSocket-Protocol"));
        if (!protocols.isEmpty())
        {
            m_subProtocol = protocols.first();
            m_subProtocolFrameFormat = FormatForSubProtocol(m_subProtocol);
        }
    }
}

TorcWebSocket::TorcWebSocket(TorcQThread *Parent, const QHostAddress &Address, quint16 Port, bool Authenticate, WSSubProtocol Protocol)
  : QObject(),
    m_parent(Parent),
    m_authenticate(Authenticate),
    m_handShaking(true),
    m_upgradeResponseReader(new TorcHTTPReader()),
    m_address(Address),
    m_port(Port),
    m_upgradeRequest(NULL),
    m_socket(NULL),
    m_abort(0),
    m_serverSide(false),
    m_readState(ReadHeader),
    m_echoTest(false),
    m_subProtocol(Protocol),
    m_subProtocolFrameFormat(FormatForSubProtocol(Protocol)),
    m_frameFinalFragment(false),
    m_frameOpCode(OpContinuation),
    m_frameMasked(false),
    m_framePayloadLength(0),
    m_frameMask(QByteArray(4, 0)),
    m_framePayload(QByteArray()),
    m_framePayloadReadPosition(0),
    m_bufferedPayload(NULL),
    m_bufferedPayloadOpCode(OpContinuation),
    m_closeReceived(false),
    m_closeSent(false),
    m_currentRequestID(1)
{
}

TorcWebSocket::~TorcWebSocket()
{
    // cancel any outstanding requests
    if (!m_currentRequests.isEmpty())
    {
        // clients should be cancelling requests before closing the connection, so this
        // may represent a leak
        LOG(VB_GENERAL, LOG_WARNING, QString("%1 outstanding RPC requests").arg(m_currentRequests.size()));

        while (!m_currentRequests.isEmpty())
            HandleCancelRequest(m_currentRequests.begin().value());
    }

    InitiateClose(CloseGoingAway, QString("WebSocket exiting normally"));

    CloseSocket();

    delete m_upgradeResponseReader;
    delete m_upgradeRequest;
    delete m_bufferedPayload;
    m_upgradeResponseReader = NULL;
    m_upgradeRequest        = NULL;
    m_bufferedPayload       = NULL;

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
    WSSubProtocol protocol = SubProtocolNone;

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
        error = "invalid Request-URI";
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

    if (valid && version != Version13 && version != Version8)
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
        QList<WSSubProtocol> protocols = SubProtocolsFromPrioritisedString(Request->Headers()->value("Sec-WebSocket-Protocol"));
        if (!protocols.isEmpty())
            protocol = protocols.first();
    }

    if (!valid)
    {
        LOG(VB_GENERAL, LOG_ERR, error);

        Request->SetStatus(HTTP_BadRequest);

        if (versionerror)
            Request->SetResponseHeader("Sec-WebSocket-Version", "8,13");

        return false;
    }

    // valid handshake so set response headers and transfer socket
    LOG(VB_GENERAL, LOG_DEBUG, "Received valid websocket Upgrade request");

    QString key = Request->Headers()->value("Sec-WebSocket-Key") + QLatin1String("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    QString nonce = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toBase64();

    Request->SetResponseType(HTTPResponseNone);
    Request->SetStatus(HTTP_SwitchingProtocols);
    Request->SetConnection(HTTPConnectionUpgrade);
    Request->SetResponseHeader("Upgrade", "websocket");
    Request->SetResponseHeader("Sec-WebSocket-Accept", nonce);
    if (!protocol == SubProtocolNone)
        Request->SetResponseHeader("Sec-WebSocket-Protocol", SubProtocolsToString(protocol));

    // if this is a Torc peer connecting, we want TorcNetworkedContext to handle this
    // socket, otherwise pass to TorcHTTPServer.
    // NB TorcNetworkedContext starts before TorcHTTPServer so we can assume gNetworkedService should be valid
    if (Request->Headers()->contains("Torc-UUID"))
        TorcNetworkedContext::UpgradeSocket(Request, Socket);
    else
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

///\brief Convert SubProtocols to HTTP readable string.
QString TorcWebSocket::SubProtocolsToString(WSSubProtocols Protocols)
{
    QStringList list;

    if (Protocols.testFlag(SubProtocolJSONRPC)) list.append(QLatin1String("torc.json-rpc"));

    return list.join(",");
}

///\brief Parse supported WSSubProtocols from Protocols.
TorcWebSocket::WSSubProtocols TorcWebSocket::SubProtocolsFromString(const QString &Protocols)
{
    WSSubProtocols protocols = SubProtocolNone;

    if (Protocols.contains(QLatin1String("torc.json-rpc"), Qt::CaseInsensitive)) protocols |= SubProtocolJSONRPC;

    return protocols;
}

///\brief Parse a prioritised list of supported WebSocket sub-protocols.
QList<TorcWebSocket::WSSubProtocol> TorcWebSocket::SubProtocolsFromPrioritisedString(const QString &Protocols)
{
    QList<WSSubProtocol> results;

    QStringList protocols = Protocols.split(",");
    for (int i = 0; i < protocols.size(); ++i)
    {
        QString protocol = protocols[i].trimmed();

        if (!QString::compare(protocol, QLatin1String("torc.json-rpc"), Qt::CaseInsensitive)) results.append(SubProtocolJSONRPC);
    }

    return results;
}

///\brief Initialise the websocket once its parent thread is ready.
void TorcWebSocket::Start(void)
{
    // connect up remote requests
    connect(this, SIGNAL(NewRequest(TorcRPCRequest*)),       this, SLOT(HandleRemoteRequest(TorcRPCRequest*)));
    connect(this, SIGNAL(RequestCancelled(TorcRPCRequest*)), this, SLOT(HandleCancelRequest(TorcRPCRequest*)));

    // server side:)
    if (m_serverSide)
    {
        if (m_upgradeRequest && m_socket)
        {
            connect(m_socket, SIGNAL(readyRead()), this, SLOT(ReadyRead()));
            connect(m_socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(Error(QAbstractSocket::SocketError)));
            if (m_parent)
                connect(m_socket, SIGNAL(disconnected()), m_parent, SLOT(quit()));

            m_upgradeRequest->Respond(m_socket, &m_abort);

            LOG(VB_GENERAL, LOG_INFO, "Server WebSocket connected to '" +
                    (m_socket->peerAddress().toString() + ":" + QString::number(m_socket->peerPort())) +
                    "' (Subprotocol: " + (SubProtocolsToString(m_subProtocol)) + ")");

            emit ConnectionEstablished();
            return;
        }
    }
    else
    {
        // guard against inappropriate usage
        delete m_socket;

        m_socket = new QTcpSocket();
        connect(m_socket, SIGNAL(connected()), this, SLOT(Connected()));
        connect(m_socket, SIGNAL(readyRead()), this, SLOT(ReadyRead()));
        connect(m_socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(Error(QAbstractSocket::SocketError)));
        if (m_parent)
            connect(m_socket, SIGNAL(disconnected()), m_parent, SLOT(quit()));

        m_socket->connectToHost(m_address, m_port);
        return;
    }

    // failed
    LOG(VB_GENERAL, LOG_ERR, "Failed to start Websocket");
    if (m_parent)
        m_parent->quit();
}

///\brief Receives notifications when a property for a subscribed service has changed.
void TorcWebSocket::PropertyChanged(void)
{
    TorcHTTPService *service = dynamic_cast<TorcHTTPService*>(sender());
    if (service && senderSignalIndex() > -1)
    {
        TorcRPCRequest *request = new TorcRPCRequest(service->Signature() + service->GetMethod(senderSignalIndex()));
        request->AddParameter("value", service->GetProperty(senderSignalIndex()));
        SendFrame(m_subProtocolFrameFormat, request->SerialiseRequest(m_subProtocol));
    }
}

bool TorcWebSocket::HandleNotification(const QString &Method)
{
    if (m_subscribers.contains(Method))
    {
        QMap<QString,QObject*>::const_iterator it = m_subscribers.find(Method);
        while (it != m_subscribers.end() && it.key() == Method)
        {
            QMetaObject::invokeMethod(it.value(), "ServiceNotification", Q_ARG(QString, Method));
            ++it;
        }

        return true;
    }

    return false;
}

/*! \brief Initiate a Remote Procedure Call.
*/
void TorcWebSocket::RemoteRequest(TorcRPCRequest *Request)
{
    if (Request)
    {
        if (Request->IsNotification())
            m_outstandingNotifications.ref();
        else
            Request->UpRef(); // NB

        emit NewRequest(Request);
    }
}

/*! \brief Cancel a Remote Procedure Call.
 *
 * Under normal operation, there is usually no need to cancel a call. When a parent exits
 * before a call is completed however, HandleCancelRequest may not be invoked before
 * the parent deletes the thread. In this case it is highly likely the Request will leak.
 * Hence we default to waiting for a short period to allow the call to complete.
 *
 * \note We assume the request will only ever be referenced by its owner and by TorcWebSocket.
*/
void TorcWebSocket::CancelRequest(TorcRPCRequest *Request, int Wait /* = 1000 ms*/)
{
    if (Request && !Request->IsNotification())
    {
        Request->AddState(TorcRPCRequest::Cancelled);
        emit RequestCancelled(Request);

        if (Wait > 0)
        {
            int count = 0;
            while (Request->IsShared() && (count++ < Wait))
                QThread::msleep(1);

            if (Request->IsShared())
                LOG(VB_GENERAL, LOG_ERR, "Request is still shared after cancellation");
        }
    }
}

/*! \brief Block until all outstanding notifications have been processed.
 *
 * Notifications have no owner (but are tracked internally by TorcWebSocket) and hence
 * we need to wait for queued notifications to be processed in HandleRemoteRequest to avoid
 * potentially leaking TorcRPCRequest's when closing the connection.
 *
 * \note This method must be called from another thread.
*/
void TorcWebSocket::WaitForNotifications(void)
{
    if (QThread::currentThread() == this->thread())
    {
        LOG(VB_GENERAL, LOG_ERR, "WaitForNotifications called from the wrong thread. Ignoring");
        return;
    }

    // wait a maximum of 1000ms
    int count = 0;
    while (count++ < 1000 && m_outstandingNotifications.fetchAndAddOrdered(0))
        QThread::msleep(1);

    if (m_outstandingNotifications.fetchAndAddOrdered(0))
        LOG(VB_GENERAL, LOG_ERR, "Outstanding notifications even after waiting 1000ms");
}

/*! \brief Thread safe Remote Procedure Call implementation.
*/
void TorcWebSocket::HandleRemoteRequest(TorcRPCRequest *Request)
{
    if (!Request)
        return;

    bool notification = Request->IsNotification();

    // guard against a request that is immediately cancelled
    if (Request->GetState() & TorcRPCRequest::Cancelled)
    {
        // NB a notification cannot be cancelled
        Request->DownRef();
    }
    else
    {
        if (!notification)
        {
            int id = m_currentRequestID++;
            while (m_currentRequests.contains(id))
                id = m_currentRequestID++;

            Request->SetID(id);
            m_currentRequests.insert(id, Request);

            // start a timer for this request
            m_requestTimers.insert(startTimer(10000, Qt::CoarseTimer), id);

            // keep id's at sane values
            if (m_currentRequestID > 100000)
                m_currentRequestID = 1;
        }

        Request->AddState(TorcRPCRequest::RequestSent);

        if (m_subProtocol == SubProtocolNone)
            LOG(VB_GENERAL, LOG_ERR, "No protocol specified for remote procedure call");
        else
            SendFrame(m_subProtocolFrameFormat, Request->SerialiseRequest(m_subProtocol));
    }

    // notifications are fire and forget, so downref immediately
    if (notification)
    {
        Request->DownRef();
        m_outstandingNotifications.deref();
    }
}

/*! \brief Thread safe cancellation of Remote Procedure Call.
*/
void TorcWebSocket::HandleCancelRequest(TorcRPCRequest *Request)
{
    if (Request && Request->GetID() > -1)
    {
        int id = Request->GetID();
        if (m_currentRequests.contains(id))
        {
            // cancel the timer
            int timer = m_requestTimers.key(id);
            killTimer(timer);
            m_requestTimers.remove(timer);

            // cancel the request
            m_currentRequests.remove(id);
            Request->DownRef();
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "Cannot cancel unknown RPC request");
        }
    }
}

/*! \brief Process incoming data
 *
 * Data for any given frame may be received over a number of packets, hence the need
 * to track the current read status.
*/
void TorcWebSocket::ReadyRead(void)
{
    while (m_socket && (m_socket->bytesAvailable() || (m_readState == ReadPayload && m_framePayloadLength == 0)) && !m_abort)
    {
        if (m_handShaking)
        {
            // read response (which is the only raw HTTP we should see)
            if (!m_upgradeResponseReader->Read(m_socket, &m_abort))
            {
                LOG(VB_GENERAL, LOG_ERR, "Error reading upgrade response");
                CloseSocket();
                return;
            }

            // response complete
            if (!m_upgradeResponseReader->m_ready)
                continue;

            // parse the response
            TorcHTTPRequest request(m_upgradeResponseReader);

            bool valid = true;
            QString error;

            // is it a response
            if (valid && request.GetHTTPType() != HTTPResponse)
            {
                valid = false;
                error = QString("Response is not an HTTP response");
            }

            // is it switching protocols
            if (valid && request.GetHTTPStatus() != HTTP_SwitchingProtocols)
            {
                valid = false;
                error = QString("Expected '%1' - got '%2'").arg(TorcHTTPRequest::StatusToString(HTTP_SwitchingProtocols))
                        .arg(TorcHTTPRequest::StatusToString(request.GetHTTPStatus()));
            }

            // does it contain the correct headers
            if (valid && !(request.Headers()->contains("Upgrade") && request.Headers()->contains("Connection") &&
                           request.Headers()->contains("Sec-WebSocket-Accept")))
            {
                valid = false;
                error = QString("Response is missing required headers");
            }

            // correct header contents
            if (valid)
            {
                QString upgrade    = request.Headers()->value("Upgrade").trimmed();
                QString connection = request.Headers()->value("Connection").trimmed();
                QString accept     = request.Headers()->value("Sec-WebSocket-Accept").trimmed();
                QString protocols  = request.Headers()->value("Sec-WebSocket-Protocol").trimmed();

                if (!upgrade.contains("websocket", Qt::CaseInsensitive) || !connection.contains("upgrade", Qt::CaseInsensitive))
                {
                    valid = false;
                    error = QString("Unexpected header content");
                }
                else
                {
                    if (!accept.contains(m_challengeResponse, Qt::CaseSensitive))
                    {
                        valid = false;
                        error = QString("Incorrect Sec-WebSocket-Accept response");
                    }
                }

                // ensure the correct subprotocol (if any) has been agreed
                if (valid)
                {
                    if (m_subProtocol == SubProtocolNone)
                    {
                        if (!protocols.isEmpty())
                        {
                            valid = false;
                            error = QString("Unexpected subprotocol");
                        }
                    }
                    else
                    {
                        WSSubProtocols subprotocols = SubProtocolsFromString(protocols);
                        if ((subprotocols | m_subProtocol) != m_subProtocol)
                        {
                            valid = false;
                            error = QString("Unexpected subprotocol");
                        }
                    }
                }
            }

            if (!valid)
            {
                LOG(VB_GENERAL, LOG_ERR, error);
                CloseSocket();
                return;
            }

            LOG(VB_GENERAL, LOG_INFO, "Received valid upgrade response - switching to frame protocol");
            m_handShaking = false;
            emit ConnectionEstablished();
        }
        else
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
                        InitiateClose(CloseUnexpectedError, QString("Read error"));
                        return;
                    }

                    m_frameFinalFragment = (header[0] & 0x80) != 0;
                    m_frameOpCode        = static_cast<OpCode>(header[0] & 0x0F);
                    m_frameMasked        = (header[1] & 0x80) != 0;
                    quint8 length        = (header[1] & 0x7F);
                    bool reservedbits    = (header[0] & 0x70) != 0;

                    // validate the header against current state and specification
                    CloseCode error = CloseNormal;
                    QString reason;

                    // invalid use of reserved bits
                    if (reservedbits)
                    {
                        reason = QString("Invalid use of reserved bits");
                        error = CloseProtocolError;
                    }

                    // control frames can only have payloads of up to 125 bytes
                    else if ((m_frameOpCode & 0x8) && length > 125)
                    {
                        reason = QString("Control frame payload too large");
                        error = CloseProtocolError;

                        // need to acknowledge when an OpClose is received
                        if (OpClose == m_frameOpCode)
                            m_closeReceived = true;
                    }

                    // if this is a server, clients must be sending masked frames and vice versa
                    else if (m_serverSide != m_frameMasked)
                    {
                        reason = QString("Masking error");
                        error = CloseProtocolError;
                    }

                    // we should never receive a reserved opcode
                    else if (m_frameOpCode != OpText && m_frameOpCode != OpBinary && m_frameOpCode != OpContinuation &&
                        m_frameOpCode != OpPing && m_frameOpCode != OpPong   && m_frameOpCode != OpClose)
                    {
                        reason = QString("Invalid use of reserved opcode");
                        error = CloseProtocolError;
                    }

                    // control frames cannot be fragmented
                    else if (!m_frameFinalFragment && (m_frameOpCode == OpPing || m_frameOpCode == OpPong || m_frameOpCode == OpClose))
                    {
                        reason = QString("Fragmented control frame");
                        error = CloseProtocolError;
                    }

                    // a continuation frame must have an opening frame
                    else if (!m_bufferedPayload && m_frameOpCode == OpContinuation)
                    {
                        reason = QString("Fragmentation error");
                        error = CloseProtocolError;
                    }

                    // only continuation frames or control frames are expected once in the middle of a frame
                    else if (m_bufferedPayload && !(m_frameOpCode == OpContinuation || m_frameOpCode == OpPing || m_frameOpCode == OpPong || m_frameOpCode == OpClose))
                    {
                        reason = QString("Fragmentation error");
                        error = CloseProtocolError;
                    }

                    // ensure OpCode matches that expected by SubProtocol
                    else if ((!m_echoTest && m_subProtocol != SubProtocolNone) &&
                             (m_frameOpCode == OpText || m_frameOpCode == OpBinary) &&
                              m_frameOpCode != m_subProtocolFrameFormat)
                    {
                        reason = QString("Received incorrect frame type for subprotocol");
                        error  = CloseUnsupportedDataType;
                    }

                    if (error != CloseNormal)
                    {
                        LOG(VB_GENERAL, LOG_ERR, QString("Read error: %1 (%2)").arg(CloseCodeToString(error)).arg(reason));
                        InitiateClose(error, reason);
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
                        InitiateClose(CloseUnexpectedError, QString("Read error"));
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
                        InitiateClose(CloseUnexpectedError, QString("Read error"));
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
                        InitiateClose(CloseUnexpectedError, QString("Read error"));
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
                            InitiateClose(CloseUnexpectedError, QString("Read error"));
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
                                HandleCloseRequest(m_framePayload);
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
                                    InitiateClose(CloseInconsistentData, "Invalid UTF-8 text");
                                }
                                else
                                {
                                    // echo test for AutoBahn test suite
                                    if (m_echoTest)
                                    {
                                        SendFrame(m_bufferedPayload ? m_bufferedPayloadOpCode : m_frameOpCode,
                                                  m_bufferedPayload ? *m_bufferedPayload : m_framePayload);
                                    }
                                    else
                                    {
                                        ProcessPayload(m_bufferedPayload ? *m_bufferedPayload : m_framePayload);
                                    }
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
                        InitiateClose(CloseUnexpectedError, QString("Read error"));
                        return;
                    }
                }
                break;
            }
        }
    }
}

/*! \brief A subscriber object has been deleted.
 *
 * If subscribers exit without unsubscribing, the subscriber list will never be cleaned up. So listen
 * for deletion signals and act if necessary.
*/
void TorcWebSocket::SubscriberDeleted(QObject *Object)
{
    QList<QString> remove;

    QMap<QString,QObject*>::const_iterator it = m_subscribers.begin();
    for ( ; it != m_subscribers.end(); ++it)
        if (it.value() == Object)
            remove.append(it.key());

    foreach (QString service, remove)
    {
        m_subscribers.remove(service, Object);
        LOG(VB_GENERAL, LOG_WARNING, QString("Removed stale subscription to '%1'").arg(service));
    }
}

void TorcWebSocket::CloseSocket(void)
{
    if (m_socket)
    {
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState && !m_socket->waitForDisconnected(1000))
            LOG(VB_GENERAL, LOG_WARNING, "WebSocket not successfully disconnected before closing");
        m_socket->close();
        m_socket->deleteLater();
        m_socket = NULL;
    }
}

void TorcWebSocket::Connected(void)
{
    if (!m_socket)
        return;

    QByteArray *upgrade = new QByteArray();
    QTextStream stream(upgrade);

    QByteArray nonce;
    for (int i = 0; i < 16; ++i)
        nonce.append(qrand() % 0x100);
    nonce = nonce.toBase64();

    // store the expected response for later comparison
    QString key = QString(nonce.data()) + QLatin1String("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    m_challengeResponse = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toBase64();

    QHostAddress host(m_address);

    stream << "GET / HTTP/1.1\r\n";
    stream << "User-Agent: " << TorcHTTPServer::PlatformName() << "\r\n";
    stream << "Host: " << TorcNetwork::IPAddressToLiteral(host, m_port) << "\r\n";
    stream << "Upgrade: WebSocket\r\n";
    stream << "Connection: Upgrade\r\n";
    stream << "Sec-WebSocket-Version: 13\r\n";
    stream << "Sec-WebSocket-Key: " << nonce.data() << "\r\n";
    stream << "Torc-UUID: " << gLocalContext->GetUuid() << "\r\n";
    stream << "Torc-Port: " << QString::number(TorcHTTPServer::GetPort()) << "\r\n";
    if (m_subProtocol != SubProtocolNone)
        stream << "Sec-WebSocket-Protocol: " << SubProtocolsToString(m_subProtocol) << "\r\n";
    if (m_authenticate)
        stream << "Authorization: " << QByteArray("Basic " + QByteArray("admin:1234").toBase64()) << "\r\n";
    stream << "\r\n";
    stream.flush();

    if (m_socket->write(upgrade->data(), upgrade->size()) != upgrade->size())
    {
        LOG(VB_GENERAL, LOG_ERR, "Unexpected write error");
        CloseSocket();
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Client WebSocket connected to '%1' (SubProtocol: %2)")
        .arg(TorcNetwork::IPAddressToLiteral(m_address, (m_port))).arg(SubProtocolsToString(m_subProtocol)));

    LOG(VB_NETWORK, LOG_DEBUG, QString("Data...\r\n%1").arg(upgrade->data()));
}

void TorcWebSocket::Error(QAbstractSocket::SocketError SocketError)
{
    if (m_socket)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("WebSocket error: %1 ('%2')").arg(m_socket->error()).arg(m_socket->errorString()));
        CloseSocket();
    }
}

bool TorcWebSocket::event(QEvent *Event)
{
    if (Event->type() == QEvent::Timer)
    {
        QTimerEvent *event = static_cast<QTimerEvent*>(Event);
        if (event)
        {
            int timerid = event->timerId();

            if (m_requestTimers.contains(timerid))
            {
                // remove the timer
                int requestid = m_requestTimers.value(timerid);
                killTimer(timerid);
                m_requestTimers.remove(timerid);

                // handle the timeout
                if (m_currentRequests.contains(requestid))
                {
                    TorcRPCRequest *request = m_currentRequests.value(requestid);
                    request->AddState(TorcRPCRequest::TimedOut);
                    LOG(VB_GENERAL, LOG_WARNING, QString("'%1' request timed out").arg(request->GetMethod()));

                    request->NotifyParent();

                    m_currentRequests.remove(requestid);
                    request->DownRef();
                }

                return true;
            }
        }
    }

    return QObject::event(Event);
}

TorcWebSocket::OpCode TorcWebSocket::FormatForSubProtocol(WSSubProtocol Protocol)
{
    switch (Protocol)
    {
        case SubProtocolNone:
            return OpText;
        case SubProtocolJSONRPC:
            return OpText;
    }

    return OpText;
}

/*! \brief Compose and send a properly formatted websocket frame.
 *
 * \note If this is a client side socket, Payload will be masked in place.
*/
void TorcWebSocket::SendFrame(OpCode Code, QByteArray &Payload)
{
    // guard against programmer error
    if (m_handShaking)
    {
        LOG(VB_GENERAL, LOG_ERR, "Trying to send frame before handshake completed");
        return;
    }

    // don't send if OpClose has already been sent or OpClose received and
    // we're sending anything other than the echoed OpClose
    if (m_closeSent || (m_closeReceived && Code != OpClose))
        return;

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

    if (m_socket && m_socket->write(frame) == frame.size())
    {
        if ((Payload.size() && m_socket->write(Payload) == Payload.size()) || !Payload.size())
        {
            m_socket->flush();
            LOG(VB_NETWORK, LOG_DEBUG, QString("Sent frame (Final), OpCode: '%1' Masked: %2 Length: %3")
                .arg(OpCodeToString(Code)).arg(!m_serverSide).arg(Payload.size()));
            return;
        }
    }

    if (Code != OpClose)
        InitiateClose(CloseUnexpectedError, QString("Send error"));
}

void TorcWebSocket::HandlePing(QByteArray &Payload)
{
    // ignore pings once in close down
    if (m_closeReceived || m_closeSent)
        return;

    SendFrame(OpPong, Payload);
}

void TorcWebSocket::HandlePong(QByteArray &Payload)
{
    // TODO validate known pings
    (void)Payload;
}

void TorcWebSocket::HandleCloseRequest(QByteArray &Close)
{
    CloseCode newclosecode = CloseNormal;
    int closecode = CloseNormal;
    QString reason;

    if (Close.size() < 1)
    {
        QByteArray newpayload;
        newpayload.append((CloseNormal >> 8) & 0xff);
        newpayload.append(CloseNormal & 0xff);
        Close = newpayload;
    }
    // payload size 1 is invalid
    else if (Close.size() == 1)
    {
        LOG(VB_GENERAL, LOG_ERR, "Invalid close payload size (<2)");
        newclosecode = CloseProtocolError;
    }
    // check close code if present
    else if (Close.size() > 1)
    {
        closecode = qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(Close.data()));

        if (closecode < CloseNormal || closecode > 4999 || closecode == CloseReserved1004 ||
            closecode == CloseStatusCodeMissing || closecode == CloseAbnormal || closecode == CloseTLSHandshakeError)
        {
            LOG(VB_GENERAL, LOG_ERR, "Invalid close code");
            newclosecode = CloseProtocolError;
        }
    }

    // check close reason if present
    if (Close.size() > 2 && newclosecode == CloseNormal)
    {
        if (!utf8::is_valid(Close.data() + 2, Close.data() + Close.size()))
        {
            LOG(VB_GENERAL, LOG_ERR, "Invalid UTF8 in close payload");
            newclosecode = CloseInconsistentData;
        }
        else
        {
            reason = QString::fromUtf8(Close.data() + 2, Close.size() -2);
        }
    }

    if (newclosecode != CloseNormal)
    {
        QByteArray newpayload;
        newpayload.append((newclosecode >> 8) & 0xff);
        newpayload.append(newclosecode & 0xff);
        Close = newpayload;
    }
    else
    {
        LOG(VB_NETWORK, LOG_INFO, QString("Received Close: %1 ('%2')").arg(CloseCodeToString((CloseCode)closecode)).arg(reason));
    }

    m_closeReceived = true;

    if (m_closeSent)
    {
        // OpClose sent and received, exit immediately
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

void TorcWebSocket::InitiateClose(CloseCode Close, const QString &Reason)
{
    if (!m_closeSent && !m_handShaking)
    {
        QByteArray payload;
        payload.append((Close >> 8) & 0xff);
        payload.append(Close & 0xff);
        payload.append(Reason.toUtf8());
        SendFrame(OpClose, payload);
        m_closeSent = true;
        CloseSocket();
    }
}

void TorcWebSocket::ProcessPayload(const QByteArray &Payload)
{
    if (m_subProtocol == SubProtocolJSONRPC)
    {
        // NB there is no method to support SENDING batched requests (hence
        // we should only receive batched requests from 3rd parties) and hence there
        // is no support for handling batched responses.
        TorcRPCRequest *request = new TorcRPCRequest(m_subProtocol, Payload, this);

        // if the request has data, we need to send it (it was a request!)
        if (!request->GetData().isEmpty())
        {
            SendFrame(m_subProtocolFrameFormat, request->GetData());
        }
        // if the request has an id, we need to process it
        else if (request->GetID() > -1)
        {
            int id = request->GetID();
            if (m_currentRequests.contains(id))
            {
                TorcRPCRequest *requestor = m_currentRequests.value(id);
                requestor->AddState(TorcRPCRequest::ReplyReceived);

                if (request->GetState() & TorcRPCRequest::Errored)
                {
                    requestor->AddState(TorcRPCRequest::Errored);
                }
                else
                {
                    QString method = requestor->GetMethod();
                    // is this a successful response to a subscription request?
                    if (method.endsWith("/Subscribe"))
                    {
                        method.chop(9);
                        QObject *parent = requestor->GetParent();

                        if (parent->metaObject()->indexOfSlot(QMetaObject::normalizedSignature("ServiceNotification(QString)")) < 0)
                        {
                            LOG(VB_GENERAL, LOG_ERR, QString("Cannot monitor subscription to '%1' for object '%2' - no notification slot").arg(method).arg(parent->objectName()));
                        }
                        else if (request->GetReply().type() == QVariant::Map)
                        {
                            // listen for destroyed signals to ensure the subscriptions are cleaned up
                            connect(parent, SIGNAL(destroyed(QObject*)), this, SLOT(SubscriberDeleted(QObject*)));

                            QVariantMap map = request->GetReply().toMap();
                            if (map.contains("properties") && map.value("properties").type() == QVariant::List)
                            {
                                QVariantList properties = map.value("properties").toList();

                                // add each notification/parent pair to the subscriber list
                                QVariantList::const_iterator it = properties.begin();
                                for ( ; it != properties.end(); ++it)
                                {
                                    if (it->type() == QVariant::Map)
                                    {
                                        QVariantMap property = it->toMap();
                                        if (property.contains("notification"))
                                        {
                                            QString service = method + property.value("notification").toString();
                                            if (m_subscribers.contains(service, parent))
                                            {
                                                LOG(VB_GENERAL, LOG_WARNING, QString("Object '%1' already has subscription to '%2'").arg(parent->objectName()).arg(service));
                                            }
                                            else
                                            {
                                                m_subscribers.insertMulti(service, parent);
                                                LOG(VB_GENERAL, LOG_INFO, QString("Object '%1' subscribed to '%2'").arg(parent->objectName()).arg(service));
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    // or a successful unsubscribe?
                    else if (method.endsWith("/Unsubscribe"))
                    {
                        method.chop(11);
                        QObject *parent = requestor->GetParent();

                        // iterate over our subscriber list and remove anything that starts with method and points to parent
                        QStringList remove;

                        QMap<QString,QObject*>::const_iterator it = m_subscribers.begin();
                        for ( ; it != m_subscribers.end(); ++it)
                            if (it.value() == parent && it.key().startsWith(method))
                                remove.append(it.key());

                        foreach (QString signature, remove)
                        {
                            LOG(VB_GENERAL, LOG_INFO, QString("Object '%1' unsubscribed from '%2'").arg(parent->objectName()).arg(signature));
                            m_subscribers.remove(signature, parent);
                        }

                        // and disconnect the destroyed signal if we have no more subscriptions for this object
                        if (!m_subscribers.values().contains(parent))
                            (void)disconnect(parent, 0, this, 0);
                    }

                    requestor->SetReply(request->GetReply());
                }

                requestor->NotifyParent();
                m_currentRequests.remove(id);
                requestor->DownRef();
            }
        }

        request->DownRef();
    }
}

TorcWebSocketThread::TorcWebSocketThread(TorcHTTPRequest *Request, QTcpSocket *Socket)
  : TorcQThread("WebSocket"),
    m_webSocket(new TorcWebSocket(this, Request, Socket))
{
    m_webSocket->moveToThread(this);
}

TorcWebSocketThread::TorcWebSocketThread(const QHostAddress &Address, quint16 Port, bool Authenticate, TorcWebSocket::WSSubProtocol Protocol)
  : TorcQThread("WebSocket"),
    m_webSocket(new TorcWebSocket(this, Address, Port, Authenticate, Protocol))
{
    m_webSocket->moveToThread(this);
}

TorcWebSocketThread::~TorcWebSocketThread()
{
    LOG(VB_GENERAL, LOG_INFO, "WebSocketThread dtor");

    delete m_webSocket;
    m_webSocket = NULL;
}

void TorcWebSocketThread::Start(void)
{
    m_webSocket->Start();
}

void TorcWebSocketThread::Finish(void)
{
}

TorcWebSocket* TorcWebSocketThread::Socket(void)
{
    return m_webSocket;
}

void TorcWebSocketThread::Shutdown(void)
{
    if (m_webSocket)
        m_webSocket->WaitForNotifications();
    disconnect();
    quit();
    wait();
}
