/* Class TorcNetworkedContext
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
#include "torcadminthread.h"
#include "torcnetwork.h"
#include "torcbonjour.h"
#include "torcevent.h"
#include "torchttpserver.h"
#include "upnp/torcupnp.h"
#include "upnp/torcssdp.h"
#include "torcwebsocket.h"
#include "torcrpcrequest.h"
#include "torcnetworkrequest.h"
#include "torcnetworkedcontext.h"

TorcNetworkedContext *gNetworkedContext = NULL;

/*! \class TorcNetworkService
 *  \brief Encapsulates information on a discovered Torc peer.
 *
 * \todo TorcNetworkService properties are marked as constant, hence changes will not be propagated to list model.
 * \todo Should retries be limited? If support is added for manually specified peers (e.g. remote) and that
 *       peer is offline, need a better approach.
*/
TorcNetworkService::TorcNetworkService(const QString &Name, const QString &UUID, int Port, const QStringList &Addresses)
  : QObject(),
    m_name(Name),
    m_uuid(UUID),
    m_port(Port),
    m_uiAddress(QString()),
    m_addresses(Addresses),
    m_startTime(0),
    m_priority(-1),
    m_apiVersion(QString()),
    m_preferredAddress(0),
    m_abort(0),
    m_getPeerDetailsRPC(NULL),
    m_getPeerDetails(NULL),
    m_webSocketThread(NULL),
    m_retryScheduled(false),
    m_retryInterval(10000)
{
    QString port = QString::number(m_port);
    for (int i = 0; i < m_addresses.size(); ++i)
    {
        // Qt5.0/5.1 network requests fail with link local IPv6 addresses (due to the scope ID). So we use
        // the host if available but otherwise try to use an IPv4 address in preference to IPv6.
        if (QHostAddress(m_addresses[i]).protocol() == QAbstractSocket::IPv4Protocol)
            m_preferredAddress = i;
        m_uiAddress += m_addresses[i] + ":" + port + " ";
    }

    m_debugString = m_addresses[m_preferredAddress] + ":" + port;
}

/*! \brief Destroy this service.
 *
 * \note When cancelling m_getPeerDetailsRPC, the call to TorcWebSocket::CancelRequest is blocking (to
 *  avoid leaking the request). Hence the request should always have been cancelled and downref'd when
 *  the call is complete. If the request happens to be processed while it is being cancelled, the request
 *  will no longer be valid for cancellation in the TorcWebSocket thread and if RequestReady is triggered in the
 *  TorcNetworkedContext thread (main thread), m_getPeerDetailsRPC will already have been released.
 *  This is because TorcWebSocket and TorcNetworkedContext process all of their updates in their own threads.
 *
 * \todo Make the call to TorcNetwork::Cancel blocking, as for TorcWebSocket::CancelRequest.
*/
TorcNetworkService::~TorcNetworkService()
{
    // cancel any outstanding requests
    m_abort = 1;

    if (m_getPeerDetailsRPC && m_webSocketThread)
    {
        m_webSocketThread->Socket()->CancelRequest(m_getPeerDetailsRPC);
        m_getPeerDetailsRPC->DownRef();
        m_getPeerDetailsRPC = NULL;
    }

    if (m_getPeerDetails)
    {
        TorcNetwork::Cancel(m_getPeerDetails);
        m_getPeerDetails->DownRef();
        m_getPeerDetails = NULL;
    }

    // delete websocket
    if (m_webSocketThread)
    {
        m_webSocketThread->Shutdown();
        delete m_webSocketThread;
        m_webSocketThread = NULL;
    }
}

/*! \brief Establish a WebSocket connection to the peer if necessary.
 *
 * A connection is only established if we have the necessary details (address, API version, priority,
 * start time etc) and this application should act as the client. If we have an address and port only
 * (e.g. as provided by an SSDP search response), then perform an HTTP query for the peer details.
 *
 * A client has a lower priority or later start time in the event of matching priorities.
*/
void TorcNetworkService::Connect(void)
{
    // clear retry flag
    m_retryScheduled = false;

    // sanity check
    if (m_addresses.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "No valid peer addresses");
        return;
    }

    // do we have details for this peer
    // 3 current possibilities:
    //  - the result of Bonjour browser - we wil have all the details from the txt records and can create a WebSocket if needed.
    //  - the result of SSDP discovery - we will have no details, so perform HTTP query and then create WebSocket if needed.
    //  - an incoming client peer WebSocket - we will have no details, so peform RPC call over WebSocket to retrieve details.

    if (m_startTime == 0 || m_apiVersion.isEmpty() || m_priority < 0)
    {
        QueryPeerDetails();
        return;
    }

    // already connected
    if (m_webSocketThread)
    {
        // notify the parent that the connection is complete
        if (gNetworkedContext)
            gNetworkedContext->Connected(this);
        return;
    }

    // lower priority peers should initiate the connection
    if (m_priority < gLocalContext->GetPriority())
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Not connecting to %1 - we have higher priority").arg(m_debugString));
        return;
    }

    // matching priority, longer running app acts as the server
    // yes - the start times could be the same...
    if (m_priority == gLocalContext->GetPriority() && m_startTime > gLocalContext->GetStartTime())
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Not connecting to %1 - we started earlier").arg(m_debugString));
        return;
    }

    // use the host if available, otherwise the preferred address (IPV4 over IPv6)
    QString host = m_host.isEmpty() ? m_addresses[m_preferredAddress] : m_host;

    LOG(VB_GENERAL, LOG_INFO, QString("Trying to connect to %1").arg(m_debugString));

    m_webSocketThread = new TorcWebSocketThread(host, m_port);
    connect(m_webSocketThread,           SIGNAL(Finished()),              this, SLOT(Disconnected()));
    connect(m_webSocketThread->Socket(), SIGNAL(ConnectionEstablished()), this, SLOT(Connected()));

    m_webSocketThread->start();
}

QString TorcNetworkService::GetName(void)
{
    return m_name;
}

QString TorcNetworkService::GetUuid(void)
{
    return m_uuid;
}

int TorcNetworkService::GetPort(void)
{
    return m_port;
}

QString TorcNetworkService::GetAddress(void)
{
    return m_uiAddress;
}

qint64 TorcNetworkService::GetStartTime(void)
{
    return m_startTime;
}

int TorcNetworkService::GetPriority(void)
{
    return m_priority;
}

QString TorcNetworkService::GetAPIVersion(void)
{
    return m_apiVersion;
}

void TorcNetworkService::Connected(void)
{
    TorcWebSocket *socket = static_cast<TorcWebSocket*>(sender());
    if (m_webSocketThread && m_webSocketThread->Socket() == socket)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Connection established with %1").arg(m_debugString));
        Connect();
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Unknown WebSocket connected...");
    }
}

void TorcNetworkService::Disconnected(void)
{
    QThread *thread = static_cast<QThread*>(sender());
    if (m_webSocketThread && m_webSocketThread == thread)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Connection with %1 closed").arg(m_debugString));
        m_webSocketThread->quit();
        m_webSocketThread->wait();
        delete m_webSocketThread;
        m_webSocketThread = NULL;

        // try and reconnect. If this is a discovered service, the socket was probably closed
        // deliberately and this object is about to be deleted anyway.
        ScheduleRetry();

        // notify the parent
        if (gNetworkedContext)
            gNetworkedContext->Disconnected(this);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Unknown WebSocket disconnected...");
    }
}

void TorcNetworkService::RequestReady(TorcNetworkRequest *Request)
{
    if (m_getPeerDetails && (Request == m_getPeerDetails))
    {
        QJsonDocument jsonresult = QJsonDocument::fromJson(Request->GetBuffer());
        m_getPeerDetails->DownRef();
        m_getPeerDetails = NULL;

        if (!jsonresult.isNull() && jsonresult.isObject())
        {
            QJsonObject object = jsonresult.object();

            if (object.contains("details"))
            {
                QJsonObject details     = object["details"].toObject();
                QJsonValueRef priority  = details["priority"];
                QJsonValueRef starttime = details["starttime"];
                QJsonValueRef version   = details["version"];

                if (!priority.isNull() && !starttime.isNull() && !version.isNull())
                {
                    m_apiVersion = version.toString();
                    m_priority   = (int)priority.toDouble();
                    m_startTime  = (qint64)starttime.toDouble();

                    Connect();
                    return;
                }
                else
                {
                    LOG(VB_GENERAL, LOG_ERR, "Failed to retrieve peer information");
                }
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, "Failed to find 'details' in peer response");
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "Error parsing API return - expecting JSON object");
        }

        LOG(VB_GENERAL, LOG_ERR, QString("Response:\r\n%1").arg(jsonresult.toJson().data()));

        // try again...
        ScheduleRetry();
    }
}

void TorcNetworkService::RequestReady(TorcRPCRequest *Request)
{
    if (!Request)
        return;

    if (Request == m_getPeerDetailsRPC)
    {
        bool success = false;
        QString message;
        int state = m_getPeerDetailsRPC->GetState();

        if (state & TorcRPCRequest::TimedOut)
        {
            message = "Timed out";
        }
        else if (state & TorcRPCRequest::Cancelled)
        {
            message = "Cancelled";
        }
        else if (state & TorcRPCRequest::Errored)
        {
            QVariantMap map  = m_getPeerDetailsRPC->GetReply().toMap();
            QVariant error = map["error"];
            if (error.type() == QVariant::Map)
            {
                QVariantMap errors = error.toMap();
                message = QString("'%1' (%2)").arg(errors.value("message").toString()).arg(errors.value("code").toInt());
            }
        }
        else if (state & TorcRPCRequest::ReplyReceived)
        {
            if (m_getPeerDetailsRPC->GetReply().type() == QVariant::Map)
            {
                QVariantMap map = m_getPeerDetailsRPC->GetReply().toMap();
                QVariant priority  = map["priority"];
                QVariant starttime = map["starttime"];
                QVariant version   = map["version"];

                if (!priority.isNull() && !starttime.isNull() && !version.isNull())
                {
                    m_apiVersion = version.toString();
                    m_priority   = (int)priority.toDouble();
                    m_startTime  = (qint64)starttime.toDouble();
                    success = true;
                }
                else
                {
                    message = "Incomplete details";
                }
            }
            else
            {
                message = QString("Unexpected variant type %1").arg(m_getPeerDetailsRPC->GetReply().type());
            }
        }

        if (!success)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Call to '%1' failed (%2)").arg(m_getPeerDetailsRPC->GetMethod()).arg(message));
            ScheduleRetry();
        }

        m_getPeerDetailsRPC->DownRef();
        m_getPeerDetailsRPC = NULL;

        if (success)
        {
            LOG(VB_GENERAL, LOG_INFO, "Reply received");
            Connect();
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Unknown request ready");
    }
}

void TorcNetworkService::ScheduleRetry(void)
{
    if (!m_retryScheduled)
    {
        QTimer::singleShot(m_retryInterval, this, SLOT(Connect()));
        m_retryScheduled = true;
    }
}

void TorcNetworkService::QueryPeerDetails(void)
{
    // this is a private method only called from Connect. No need to validate m_addresses or current details.

    // use the host if available, otherwise the preferred address (IPV4 over IPv6)
    QString host = m_host.isEmpty() ? m_addresses[m_preferredAddress] : m_host;

    if (!m_webSocketThread)
    {
        if (m_getPeerDetails)
        {
            LOG(VB_GENERAL, LOG_ERR, "Already running GetDetails HTTP request");
            return;
        }

        LOG(VB_GENERAL, LOG_INFO, "Querying peer details over HTTP");

        QUrl url(host);
        url.setPort(m_port);
        url.setScheme("http");
        url.setPath("/services/GetDetails");

        QNetworkRequest networkrequest(url);
        networkrequest.setRawHeader("Accept", "application/json");

        m_getPeerDetails = new TorcNetworkRequest(networkrequest, QNetworkAccessManager::GetOperation, 0, &m_abort);
        TorcNetwork::GetAsynchronous(m_getPeerDetails, this);

        return;
    }

    // perform JSON-RPC call over socket
    if (m_getPeerDetailsRPC)
    {
        LOG(VB_GENERAL, LOG_ERR, "Already running GetDetails RPC");
        return;
    }

    m_getPeerDetailsRPC = new TorcRPCRequest("/services/GetDetails", this);
    RemoteRequest(m_getPeerDetailsRPC);
}

void TorcNetworkService::CreateSocket(TorcHTTPRequest *Request, QTcpSocket *Socket)
{
    if (!Request || !Socket)
        return;

    // guard against incorrect use
    if (m_webSocketThread)
    {
        LOG(VB_GENERAL, LOG_ERR, "Already have websocket - deleting new request");
        delete Request;
        Socket->disconnectFromHost();
        if (Socket->state() != QAbstractSocket::UnconnectedState && !Socket->waitForDisconnected(1000))
            LOG(VB_GENERAL, LOG_WARNING, "WebSocket not successfully disconnected before closing");
        Socket->close();
        Socket->deleteLater();
        return;
    }

    // create the socket
    m_webSocketThread = new TorcWebSocketThread(Request, Socket);
    Socket->moveToThread(m_webSocketThread);
    connect(m_webSocketThread,           SIGNAL(Finished()),              this, SLOT(Disconnected()));
    connect(m_webSocketThread->Socket(), SIGNAL(ConnectionEstablished()), this, SLOT(Connected()));

    m_webSocketThread->start();
}

void TorcNetworkService::RemoteRequest(TorcRPCRequest *Request)
{
    if (!Request)
        return;

    if (m_webSocketThread && m_webSocketThread->Socket())
        m_webSocketThread->Socket()->RemoteRequest(Request);
    else
        LOG(VB_GENERAL, LOG_ERR, "Cannot fulfill remote request - not connected");
}

void TorcNetworkService::CancelRequest(TorcRPCRequest *Request)
{
    if (!Request)
        return;

    if (m_webSocketThread && m_webSocketThread->Socket())
        m_webSocketThread->Socket()->CancelRequest(Request);
    else
        LOG(VB_GENERAL, LOG_ERR, "Cannot cancel request - not connected");
}

QStringList TorcNetworkService::GetAddresses(void)
{
    return m_addresses;
}

void TorcNetworkService::SetHost(const QString &Host)
{
    m_host = Host;
    m_debugString = m_host + ":" + QString::number(m_port);
}

void TorcNetworkService::SetStartTime(qint64 StartTime)
{
    m_startTime = StartTime;
}

void TorcNetworkService::SetPriority(int Priority)
{
    m_priority = Priority;
}

void TorcNetworkService::SetAPIVersion(const QString &Version)
{
    m_apiVersion = Version;
}

/*! \class TorcNetworkedContext
 *
 * ![](../images/peer-decisiontree.svg) "Torc peer discovery and connection"
 *
 * \sa TorcNetworkService
 */
TorcNetworkedContext::TorcNetworkedContext()
  : QAbstractListModel(),
    m_bonjourBrowserReference(0)
{
    // listen for events
    gLocalContext->AddObserver(this);

    // connect signals
    connect(this, SIGNAL(NewRequest(QString,TorcRPCRequest*)), this, SLOT(HandleNewRequest(QString,TorcRPCRequest*)));
    connect(this, SIGNAL(RequestCancelled(QString,TorcRPCRequest*)), this, SLOT(HandleCancelRequest(QString,TorcRPCRequest*)));
    connect(this, SIGNAL(UpgradeRequest(TorcHTTPRequest*,QTcpSocket*)), this, SLOT(HandleUpgrade(TorcHTTPRequest*,QTcpSocket*)));

#if defined(CONFIG_LIBDNS_SD) && CONFIG_LIBDNS_SD
    // always create the global instance
    TorcBonjour::Instance();

    // but immediately suspend if network access is disallowed
    if (!TorcNetwork::IsAllowed())
        TorcBonjour::Suspend(true);
#endif

    // start browsing early for other Torc applications
    // Torc::Client implies it is a consumer of media - may need revisiting
    if (gLocalContext->FlagIsSet(Torc::Client))
    {
#if defined(CONFIG_LIBDNS_SD) && CONFIG_LIBDNS_SD
        m_bonjourBrowserReference = TorcBonjour::Instance()->Browse("_torc._tcp.");
#endif

        // NB TorcSSDP singleton isn't running yet - but the request will be queued
        TorcSSDP::Search(TORC_ROOT_UPNP_DEVICE, this);
    }

    TorcUPNPDescription upnp(QString("uuid:%1").arg(gLocalContext->GetUuid()), TORC_ROOT_UPNP_DEVICE, "LOCATION", 1000);
    TorcSSDP::Announce(upnp);
}

TorcNetworkedContext::~TorcNetworkedContext()
{
    TorcUPNPDescription upnp(QString("uuid:%1").arg(gLocalContext->GetUuid()), TORC_ROOT_UPNP_DEVICE, "LOCATION", 1000);
    TorcSSDP::CancelAnnounce(upnp);

    // stoplistening
    gLocalContext->RemoveObserver(this);

    // cancel upnp search
    TorcSSDP::CancelSearch(TORC_ROOT_UPNP_DEVICE, this);

#if defined(CONFIG_LIBDNS_SD) && CONFIG_LIBDNS_SD
    // stop browsing for torc._tcp
    if (m_bonjourBrowserReference)
        TorcBonjour::Instance()->Deregister(m_bonjourBrowserReference);
    m_bonjourBrowserReference = 0;

    // N.B. We delete the global instance here
    TorcBonjour::TearDown();
#endif

    while (!m_discoveredServices.isEmpty())
        delete m_discoveredServices.takeLast();
}

QVariant TorcNetworkedContext::data(const QModelIndex &Index, int Role) const
{
    int row = Index.row();

    if (row < 0 || row >= m_discoveredServices.size() || Role != Qt::DisplayRole)
        return QVariant();

    return QVariant::fromValue(m_discoveredServices.at(row));
}

QHash<int,QByteArray> TorcNetworkedContext::roleNames(void) const
{
    QHash<int,QByteArray> roles;
    roles.insert(Qt::DisplayRole, "m_name");
    roles.insert(Qt::DisplayRole, "m_uuid");
    roles.insert(Qt::DisplayRole, "m_port");
    roles.insert(Qt::DisplayRole, "m_uiaddress");
    return roles;
}

int TorcNetworkedContext::rowCount(const QModelIndex&) const
{
    return m_discoveredServices.size();
}

void TorcNetworkedContext::Connected(TorcNetworkService *Peer)
{
    if (!Peer)
        return;

    emit PeerConnected(Peer->GetName(), Peer->GetUuid());
}

void TorcNetworkedContext::Disconnected(TorcNetworkService *Peer)
{
    if (!Peer)
        return;

    emit PeerDisconnected(Peer->GetName(), Peer->GetUuid());
}

bool TorcNetworkedContext::event(QEvent *Event)
{
    if (Event->type() == TorcEvent::TorcEventType)
    {
        TorcEvent *event = static_cast<TorcEvent*>(Event);
        if (event && (event->GetEvent() == Torc::ServiceDiscovered || event->GetEvent() == Torc::ServiceWentAway))
        {
            if (event->Data().contains("txtrecords"))
            {
#if defined(CONFIG_LIBDNS_SD) && CONFIG_LIBDNS_SD
                // txtrecords is Bonjour specific
                QMap<QByteArray,QByteArray> records = TorcBonjour::TxtRecordToMap(event->Data().value("txtrecords").toByteArray());

                if (records.contains("uuid"))
                {
                    QByteArray uuid  = records.value("uuid");

                    if (event->GetEvent() == Torc::ServiceDiscovered && !m_serviceList.contains(uuid) &&
                        uuid != gLocalContext->GetUuid().toLatin1())
                    {
                        QByteArray name       = event->Data().value("name").toByteArray();
                        QStringList addresses = event->Data().value("addresses").toStringList();
                        QByteArray txtrecords = event->Data().value("txtrecords").toByteArray();
                        QMap<QByteArray,QByteArray> map = TorcBonjour::TxtRecordToMap(txtrecords);
                        QString version       = QString(map.value("apiversion"));
                        qint64 starttime      = map.value("starttime").toULongLong();
                        int priority          = map.value("priority").toInt();
                        QString host          = event->Data().value("host").toString();

                        // create the new peer
                        TorcNetworkService *service = new TorcNetworkService(name, uuid, event->Data().value("port").toInt(), addresses);
                        service->SetAPIVersion(version);
                        service->SetPriority(priority);
                        service->SetStartTime(starttime);
                        service->SetHost(host);

                        // and insert into the list model
                        int position = m_discoveredServices.size();
                        beginInsertRows(QModelIndex(), position, position);
                        m_discoveredServices.append(service);
                        endInsertRows();

                        m_serviceList.append(uuid);

                        LOG(VB_GENERAL, LOG_INFO, QString("New Torc peer '%1'").arg(name.data()));

                        // try and connect - the txt records should have given us everything we need to know
                        service->Connect();
                    }
                    else if (event->GetEvent() == Torc::ServiceWentAway && m_serviceList.contains(uuid))
                    {
                        (void)m_serviceList.removeAll(uuid);

                        for (int i = 0; i < m_discoveredServices.size(); ++i)
                        {
                            if (m_discoveredServices.at(i)->GetUuid() == uuid)
                            {
                                // remove the item from the model
                                beginRemoveRows(QModelIndex(), i, i);
                                delete m_discoveredServices.takeAt(i);
                                endRemoveRows();

                                break;
                            }
                        }

                        LOG(VB_GENERAL, LOG_INFO, QString("Torc peer %1 went away").arg(uuid.data()));
                    }
                }
#endif
            }
            else if (event->Data().contains("usn"))
            {
                // USN == Unique Service Name (UPnP)
                QString uuid = TorcUPNP::UUIDFromUSN(event->Data().value("usn").toString());
            }
        }
    }

    return QAbstractListModel::event(Event);
}

/*! \brief Respond to a valid WebSocket upgrade request and schedule creation of a WebSocket on the give QTcpSocket
 *
 * \sa TorcWebSocket
 * \sa TorcHTTPServer::UpgradeSocket
*/
void TorcNetworkedContext::UpgradeSocket(TorcHTTPRequest *Request, QTcpSocket *Socket)
{
    if (!Request || !Socket)
        return;

    if (!gNetworkedContext)
    {
        LOG(VB_GENERAL, LOG_ERR, "Upgrade request but no TorcNetworkedContext singleton");
        return;
    }

    // 'push' the socket into the correct thread
    Socket->moveToThread(gNetworkedContext->thread());

    // and create the WebSocket in the correct thread
    emit gNetworkedContext->UpgradeRequest(Request, Socket);
}

/// \brief Pass Request to the remote connection identified by UUID
void TorcNetworkedContext::RemoteRequest(const QString &UUID, TorcRPCRequest *Request)
{
    if (!Request || UUID.isEmpty())
        return;

    if (!gNetworkedContext)
    {
        LOG(VB_GENERAL, LOG_ERR, "RemoteRequest but no TorcNetworkedContext singleton");
        return;
    }

    emit gNetworkedContext->NewRequest(UUID, Request);
}

/*! \brief Cancel Request associated with the connection identified by UUID
 *
 * See TorcWebSocket::CancelRequest for details on blocking.
 *
 * \sa TorcWebSocket::CancelRequest
*/
void TorcNetworkedContext::CancelRequest(const QString &UUID, TorcRPCRequest *Request, int Wait /*= 1000ms*/)
{
    if (!Request || UUID.isEmpty())
        return;

    if (!gNetworkedContext)
    {
        LOG(VB_GENERAL, LOG_ERR, "CancelRequest but no TorcNetworkedContext singleton");
        return;
    }

    if (Request && !Request->IsNotification())
    {
        Request->AddState(TorcRPCRequest::Cancelled);
        emit gNetworkedContext->RequestCancelled(UUID, Request);

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

void TorcNetworkedContext::HandleNewRequest(const QString &UUID, TorcRPCRequest *Request)
{
    if (!UUID.isEmpty() && m_serviceList.contains(UUID))
    {
        for (int i = 0; i < m_discoveredServices.size(); ++i)
        {
            if (m_discoveredServices[i]->GetUuid() == UUID)
            {
                m_discoveredServices[i]->RemoteRequest(Request);
                return;
            }
        }
    }

    LOG(VB_GENERAL, LOG_WARNING, QString("Connection identified by '%1' unknown").arg(UUID));
}

void TorcNetworkedContext::HandleCancelRequest(const QString &UUID, TorcRPCRequest *Request)
{
    if (!UUID.isEmpty() && m_serviceList.contains(UUID))
    {
        for (int i = 0; i < m_discoveredServices.size(); ++i)
        {
            if (m_discoveredServices[i]->GetUuid() == UUID)
            {
                m_discoveredServices[i]->CancelRequest(Request);
                return;
            }
        }
    }

    LOG(VB_GENERAL, LOG_WARNING, QString("Connection identified by '%1' unknown").arg(UUID));
}

void TorcNetworkedContext::HandleUpgrade(TorcHTTPRequest *Request, QTcpSocket *Socket)
{
    if (!Request || !Socket)
        return;

    QString uuid = Request->Headers()->value("Torc-UUID").trimmed();
    TorcNetworkService *service = NULL;

    if (!uuid.isEmpty() && m_serviceList.contains(uuid))
    {
        for (int i = 0; i < m_discoveredServices.size(); ++i)
        {
            if (m_discoveredServices[i]->GetUuid() == uuid)
            {
                service = m_discoveredServices[i];
                LOG(VB_GENERAL, LOG_INFO, QString("Received WebSocket for known peer ('%1')").arg(service->GetName()));
                break;
            }
        }
    }
    else if (!uuid.isEmpty())
    {
        QString name;
        QString agent = Request->Headers()->value("User-Agent").trimmed();
        int index = agent.indexOf(',');
        if (index > -1)
            name = agent.left(index);

        LOG(VB_GENERAL, LOG_INFO, QString("Received WebSocket for new peer ('%1')").arg(name));
        QStringList address;
        address.append(Socket->peerAddress().toString());
        service = new TorcNetworkService(name, uuid, Socket->peerPort(), address);
        m_serviceList.append(uuid);
        m_discoveredServices.append(service);
    }

    if (!service)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to fulfill upgrade request - deleting");
        delete Request;
        Socket->disconnectFromHost();
        if (Socket->state() != QAbstractSocket::UnconnectedState && !Socket->waitForDisconnected(1000))
            LOG(VB_GENERAL, LOG_WARNING, "WebSocket not successfully disconnected before closing");
        Socket->close();
        Socket->deleteLater();
    }

    // create the socket
    service->CreateSocket(Request, Socket);
}

static class TorcNetworkedContextObject : public TorcAdminObject
{
  public:
    TorcNetworkedContextObject()
      : TorcAdminObject(TORC_ADMIN_HIGH_PRIORITY + 1 /* start after network and before http server */)
    {
        qRegisterMetaType<TorcRPCRequest*>();
    }

    ~TorcNetworkedContextObject()
    {
        Destroy();
    }

    void Create(void)
    {
        Destroy();

        gNetworkedContext = new TorcNetworkedContext();
    }

    void Destroy(void)
    {
        delete gNetworkedContext;
        gNetworkedContext = NULL;
    }

} TorcNetworkedContextObject;
