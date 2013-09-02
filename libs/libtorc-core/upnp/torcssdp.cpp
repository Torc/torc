/* Class TorcSSDP
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
#include <QHostAddress>
#include <QUdpSocket>

// Torc
#include "torclocalcontext.h"
#include "torccoreutils.h"
#include "torclogging.h"
#include "torcadminthread.h"
#include "torcnetwork.h"
#include "torcqthread.h"
#include "torcupnp.h"
#include "torcssdp.h"

/*! \class TorcSSDPPriv
 *  \brief The internal handler for all Simple Service Discovery Protocol messaging
 *
 * \todo Revisit behaviour for search and announce wrt network availability and network allowed inbound/outbound
 * \todo Actually announce (with random 0-100ms delay) plus retry
 * \todo Schedule announce refreshes (half of cache-control)
 * \todo Respond to search requests
 * \todo Correct handling of multiple responses (i.e. via both IPv4 and IPv6)
*/

class TorcSSDPPriv
{
  public:
    TorcSSDPPriv(TorcSSDP *Parent);
    ~TorcSSDPPriv();

    void         Start                (void);
    void         Stop                 (void);
    void         Search               (const QString &Type, QObject* Owner);
    void         CancelSearch         (const QString &Type, QObject* Owner);
    void         Announce             (const TorcUPNPDescription &Description);
    void         CancelAnnounce       (const TorcUPNPDescription &Description);
    void         Read                 (QUdpSocket *Socket);
    void         ProcessDevice        (const QString &USN, const QString &Type, const QString &Location, qint64 Expires, bool Add);
    void         Refresh              (void);

  protected:
    TorcSSDP                          *m_parent;
    bool                               m_started;
    QList<QHostAddress>                m_addressess;
    QHostAddress                       m_ipv4GroupAddress;
    QUdpSocket                        *m_ipv4SearchSocket;
    QUdpSocket                        *m_ipv4MulticastSocket;
    QString                            m_ipv6LinkGroupBaseAddress;
    QHostAddress                       m_ipv6LinkGroupAddress;
    QUdpSocket                        *m_ipv6LinkSearchSocket;
    QUdpSocket                        *m_ipv6LinkMulticastSocket;
    QHash<QString,TorcUPNPDescription> m_discoveredDevices;
    QMultiHash<QString,QObject*>       m_searchRequests;
};

TorcSSDP* gSSDP = NULL;
QMutex*   gSSDPLock = new QMutex(QMutex::Recursive);
QMap<QString,QObject*> gQueuedSearches;
QList<TorcUPNPDescription> gQueuedAnnouncements;

TorcSSDPPriv::TorcSSDPPriv(TorcSSDP *Parent)
  : m_parent(Parent),
    m_started(false),
    m_ipv4GroupAddress("239.255.255.250"),
    m_ipv4SearchSocket(NULL),
    m_ipv4MulticastSocket(NULL),
    m_ipv6LinkGroupBaseAddress("FF02::C"),
    m_ipv6LinkGroupAddress(m_ipv6LinkGroupBaseAddress),
    m_ipv6LinkSearchSocket(NULL),
    m_ipv6LinkMulticastSocket(NULL)
{
    if (TorcNetwork::IsAvailable())
        Start();

    {
        QMutexLocker locker(gSSDPLock);

        if (!gQueuedSearches.isEmpty())
        {
            QMap<QString,QObject*>::const_iterator it = gQueuedSearches.begin();
            for ( ; it != gQueuedSearches.end(); ++it)
                Search(it.key(), it.value());

            gQueuedSearches.clear();
        }

        while (!gQueuedAnnouncements.isEmpty())
            Announce(gQueuedAnnouncements.takeFirst());
    }
}

TorcSSDPPriv::~TorcSSDPPriv()
{
    Stop();
}

QUdpSocket* CreateSearchSocket(const QHostAddress &HostAddress, TorcSSDP *Parent)
{
    QUdpSocket *socket = new QUdpSocket();
    if (socket->bind(HostAddress, 0))
    {
        socket->setSocketOption(QAbstractSocket::MulticastTtlOption, 4);
        socket->setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1);
        QObject::connect(socket, SIGNAL(readyRead()), Parent, SLOT(Read()));
        LOG(VB_NETWORK, LOG_INFO, "SSDP search socket " + socket->localAddress().toString() + ":" + QString::number(socket->localPort()));
        return socket;
    }

    LOG(VB_GENERAL, LOG_ERR, QString("Failed to bind %1 SSDP search socket (%2)")
        .arg(HostAddress.protocol() == QAbstractSocket::IPv6Protocol ? "IPv6" : "IPv4").arg(socket->errorString()));
    delete socket;
    return NULL;
}

QUdpSocket* CreateMulticastSocket(const QHostAddress &HostAddress, TorcSSDP *Parent, const QNetworkInterface &Interface)
{
    QUdpSocket *socket = new QUdpSocket();
    if (socket->bind(HostAddress, 1900, QUdpSocket::ShareAddress))
    {
        if (socket->joinMulticastGroup(HostAddress, Interface))
        {
            QObject::connect(socket, SIGNAL(readyRead()), Parent, SLOT(Read()));
            LOG(VB_NETWORK, LOG_INFO, "SSDP multicast socket " + socket->localAddress().toString() + ":" + QString::number(socket->localPort()));
            return socket;
        }

        LOG(VB_GENERAL, LOG_ERR, QString("Failed to join multicast group (%1)").arg(socket->errorString()));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to bind %1 SSDP multicast socket (%2)")
            .arg(HostAddress.protocol() == QAbstractSocket::IPv6Protocol ? "IPv6" : "IPv4").arg(socket->errorString()));
    }

    delete socket;
    return NULL;
}

void TorcSSDPPriv::Start(void)
{
    Stop();

    LOG(VB_GENERAL, LOG_INFO, "Starting SSDP discovery");

    // get the interface the network is using
    QNetworkInterface interface = TorcNetwork::GetInterface();

    // create sockets
    m_ipv6LinkGroupAddress      = QHostAddress(m_ipv6LinkGroupBaseAddress + "%" + interface.name());
    m_ipv4SearchSocket          = CreateSearchSocket(QHostAddress("0.0.0.0"), m_parent);
    m_ipv4MulticastSocket       = CreateMulticastSocket(m_ipv4GroupAddress, m_parent, interface);
    m_ipv6LinkSearchSocket      = CreateSearchSocket(QHostAddress("::"), m_parent);
    m_ipv6LinkMulticastSocket   = CreateMulticastSocket(m_ipv6LinkGroupAddress, m_parent, interface);

    // refresh the list of local ip addresses
    QList<QNetworkAddressEntry> entries = interface.addressEntries();
    foreach (QNetworkAddressEntry entry, entries)
        m_addressess << entry.ip();

    m_started = true;

    // search is evented from TorcSSDP parent..
}

void TorcSSDPPriv::Stop(void)
{
    // Network is no longer available - notify clients that services have gone away
    QMultiHash<QString,QObject*>::iterator it = m_searchRequests.begin();
    for ( ; it != m_searchRequests.end(); ++it)
    {
        QHash<QString,TorcUPNPDescription>::const_iterator it2 = m_discoveredDevices.begin();
        for ( ; it2 != m_discoveredDevices.end(); ++it2)
        {
            if (it2.value().GetType() == it.key())
            {
                QVariantMap data;
                data.insert("usn", it2.value().GetUSN());
                TorcEvent *event = new TorcEvent(Torc::ServiceWentAway, data);
                QCoreApplication::postEvent(it.value(), event);
            }
        }
    }

    m_discoveredDevices.clear();

    m_addressess.clear();

    if (m_started)
        LOG(VB_GENERAL, LOG_INFO, "Stopping SSDP discovery");
    m_started = false;

    if (m_ipv4SearchSocket)
        m_ipv4SearchSocket->close();
    if (m_ipv4MulticastSocket)
        m_ipv4MulticastSocket->close();
    if (m_ipv6LinkSearchSocket)
        m_ipv6LinkSearchSocket->close();
    if (m_ipv6LinkMulticastSocket)
        m_ipv6LinkMulticastSocket->close();

    m_parent->disconnect();

    delete m_ipv4SearchSocket;
    delete m_ipv4MulticastSocket;
    delete m_ipv6LinkSearchSocket;
    delete m_ipv6LinkMulticastSocket;

    m_ipv4MulticastSocket     = NULL;
    m_ipv4SearchSocket        = NULL;
    m_ipv6LinkSearchSocket    = NULL;
    m_ipv6LinkMulticastSocket = NULL;
}

void TorcSSDPPriv::Search(const QString &Type, QObject *Owner)
{
    if (Type.isEmpty() || !Owner)
    {
        // full search

        if (m_ipv4SearchSocket && m_ipv4SearchSocket->isValid() && m_ipv4SearchSocket->state() == QAbstractSocket::BoundState)
        {
            QByteArray search("M-SEARCH * HTTP/1.1\r\n"
                              "HOST: 239.255.255.250:1900\r\n"
                              "MAN: \"ssdp:discover\"\r\n"
                              "MX: 3\r\n"
                              "ST: ssdp:all\r\n\r\n");

            qint64 sent = m_ipv4SearchSocket->writeDatagram(search, m_ipv4GroupAddress, 1900);
            if (sent != search.size())
                LOG(VB_GENERAL, LOG_ERR, QString("Error sending search request (%1)").arg(m_ipv4SearchSocket->errorString()));
            else
                LOG(VB_NETWORK, LOG_INFO, "Sent IPv4 SSDP search request");
        }

        if (m_ipv6LinkSearchSocket && m_ipv6LinkSearchSocket->isValid() && m_ipv6LinkSearchSocket->state() == QAbstractSocket::BoundState)
        {
            QByteArray search("M-SEARCH * HTTP/1.1\r\n"
                              "HOST: [FF02::C]:1900\r\n"
                              "MAN: \"ssdp:discover\"\r\n"
                              "MX: 3\r\n"
                              "ST: ssdp:all\r\n\r\n");

            qint64 sent = m_ipv6LinkSearchSocket->writeDatagram(search, m_ipv6LinkGroupAddress, 1900);
            if (sent != search.size())
                LOG(VB_GENERAL, LOG_ERR, QString("Error sending search request (%1)").arg(m_ipv6LinkSearchSocket->errorString()));
            else
                LOG(VB_NETWORK, LOG_INFO, "Sent IPv6 SSDP search request");
        }

        return;
    }

    // search request (NB we don't search specifically, just use the cache)

    if (!m_searchRequests.contains(Type, Owner))
    {
        m_searchRequests.insert(Type, Owner);
        LOG(VB_NETWORK, LOG_INFO, QString("Starting search for '%1'").arg(Type));
    }

    // notify existing matches to owner
    QHash<QString,TorcUPNPDescription>::const_iterator it = m_discoveredDevices.begin();
    for ( ; it != m_discoveredDevices.end(); ++it)
    {
        if (it.value().GetType() == Type)
        {
            QVariantMap data;
            data.insert("usn",      it.value().GetUSN());
            data.insert("type",     it.value().GetType());
            data.insert("location", it.value().GetLocation());
            TorcEvent *event = new TorcEvent(Torc::ServiceDiscovered, data);
            QCoreApplication::postEvent(Owner, event);
        }
    }
}

void TorcSSDPPriv::CancelSearch(const QString &Type, QObject *Owner)
{
    if (m_searchRequests.contains(Type, Owner))
    {
        m_searchRequests.remove(Type, Owner);
        LOG(VB_NETWORK, LOG_INFO, QString("Cancelled search for '%1'").arg(Type));
    }
}

void TorcSSDPPriv::Announce(const TorcUPNPDescription &Description)
{
    LOG(VB_GENERAL, LOG_INFO, "ANNOUNCE " + Description.GetType());
}

void TorcSSDPPriv::CancelAnnounce(const TorcUPNPDescription &Description)
{
    LOG(VB_GENERAL, LOG_INFO, "CANCEL ANNOUNCE " + Description.GetType());
}

qint64 GetExpiryTime(const QString &Expires)
{
    // NB this ignores the date header - just assume it is correct
    int index = Expires.indexOf("max-age", 0, Qt::CaseInsensitive);
    if (index > -1)
    {
        int index2 = Expires.indexOf("=", index);
        if (index2 > -1)
        {
            int seconds = Expires.mid(index2 + 1).toInt();
            return QDateTime::currentMSecsSinceEpoch() + 1000 * seconds;
        }
    }

    return -1;
}

void TorcSSDPPriv::Read(QUdpSocket *Socket)
{
    while (Socket && Socket->hasPendingDatagrams())
    {
        QHostAddress address;
        quint16 port;
        QByteArray datagram;
        datagram.resize(Socket->pendingDatagramSize());
        Socket->readDatagram(datagram.data(), datagram.size(), &address, &port);

        // filter out our own announcements
        if ((m_ipv4SearchSocket && (port == m_ipv4SearchSocket->localPort())) || (m_ipv6LinkSearchSocket && (port == m_ipv6LinkSearchSocket->localPort())))
            if (m_addressess.contains(address))
                continue;

        // use a QString for greater flexibility in splitting and searching text
        QString data(datagram);
        LOG(VB_NETWORK, LOG_DEBUG, "Raw datagram:\r\n" + data);

        // split data into lines
        QStringList lines = data.split("\r\n", QString::SkipEmptyParts);
        if (!lines.isEmpty())
        {
            QMap<QString,QString> headers;

            // pull out type
            QString type = lines.takeFirst().trimmed();

            // process headers
            while (!lines.isEmpty())
            {
                QString header = lines.takeFirst();
                int index = header.indexOf(':');
                if (index > -1)
                    headers.insert(header.left(index).trimmed().toLower(), header.mid(index + 1).trimmed());
            }

            if (type.startsWith("HTTP", Qt::CaseInsensitive))
            {
                // response
                if (headers.contains("cache-control"))
                {
                    qint64 expires = GetExpiryTime(headers.value("cache-control"));
                    if (expires > 0)
                        ProcessDevice(headers.value("usn"), headers.value("st"), headers.value("location"), expires, true/*add*/);
                }
            }
            else if (type.startsWith("NOTIFY", Qt::CaseInsensitive))
            {
                // notfification ssdp:alive or ssdp:bye
                if (headers.contains("nts"))
                {
                    bool add = headers["nts"] == "ssdp:alive";

                    if (add)
                    {
                        if (headers.contains("cache-control"))
                        {
                            qint64 expires = GetExpiryTime(headers.value("cache-control"));
                            if (expires > 0)
                                ProcessDevice(headers.value("usn"), headers.value("nt"), headers.value("location"), expires, true/*add*/);
                        }
                    }
                    else
                    {
                        ProcessDevice(headers.value("usn"), headers.value("nt"), headers.value("location"), 1, false/*remove*/);
                    }
                }
            }
            else if (type.startsWith("M-SEARCH", Qt::CaseInsensitive))
            {
                // search
            }
        }
    }
}

void TorcSSDPPriv::ProcessDevice(const QString &USN, const QString &Type, const QString &Location, qint64 Expires, bool Add)
{
    if (USN.isEmpty() || Type.isEmpty() || Location.isEmpty() || Expires < 1)
        return;

    bool notify = true;

    if (m_discoveredDevices.contains(USN))
    {
        // is this just an expiry update?
        if (Add)
        {
            TorcUPNPDescription &desc = m_discoveredDevices[USN];
            if (desc.GetLocation() == Location && desc.GetType() == Type)
            {
                notify = false;
            }
            else
            {
                // TODO BtHomeHub sends two different location urls for the devices (same USN and ST)
                // One uses a 'upnp' sub-directory and the other 'dslforum' - and the xml is different.
                (void)m_discoveredDevices.remove(USN);
                m_discoveredDevices.insert(USN, TorcUPNPDescription(USN, Type, Location, Expires));
                LOG(VB_NETWORK, LOG_DEBUG, "Updated USN: " + USN);
            }
        }
        else
        {
            (void)m_discoveredDevices.remove(USN);
            LOG(VB_NETWORK, LOG_DEBUG, "Removed USN: " + USN);
        }
    }
    else if (Add)
    {
        m_discoveredDevices.insert(USN, TorcUPNPDescription(USN, Type, Location, Expires));
        LOG(VB_NETWORK, LOG_DEBUG, "Added USN: " + USN);
    }

    // update interested parties
    if (notify && m_searchRequests.contains(Type))
    {
        QVariantMap data;
        data.insert("usn", USN);

        if (Add)
        {
            data.insert("type", Type);
            data.insert("location", Location);
        }

        TorcEvent event(Add ? Torc::ServiceDiscovered : Torc::ServiceWentAway, data);

        QMultiHash<QString,QObject*>::const_iterator it = m_searchRequests.find(Type);
        while (it != m_searchRequests.end() && it.key() == Type)
        {
            QCoreApplication::postEvent(it.value(), event.Copy());
            ++it;
        }
    }
}

void TorcSSDPPriv::Refresh(void)
{
    if (!m_started)
        return;

    int count = 0;
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // remove stale discovered devices (if still present, they should have notified
    // a refresh)
    QList<TorcUPNPDescription> removed;
    QMutableHashIterator<QString,TorcUPNPDescription> it(m_discoveredDevices);
    while (it.hasNext())
    {
        it.next();
        if (it.value().GetExpiry() < now)
        {
            removed << it.value();
            it.remove();
            count++;
        }
    }

    // notify interested parties that they've been removed
    if (!removed.isEmpty())
    {
        QMultiHash<QString,QObject*>::iterator it = m_searchRequests.begin();
        for ( ; it != m_searchRequests.end(); ++it)
        {
            QList<TorcUPNPDescription>::const_iterator it2 = removed.begin();
            for ( ; it2 != removed.end(); ++it2)
            {
                if ((*it2).GetType() == it.key())
                {
                    QVariantMap data;
                    data.insert("usn", (*it2).GetUSN());
                    TorcEvent *event = new TorcEvent(Torc::ServiceWentAway, data);
                    QCoreApplication::postEvent(it.value(), event);
                }
            }
        }
    }

    LOG(VB_NETWORK, LOG_INFO, QString("Removed %1 stale cache entries").arg(count));
}

/*! \class TorcSSDP
 *  \brief The public class for handling Simple Service Discovery Protocol searches and announcements
 *
 * All SSDP interaction is via the static methods Search, CancelSearch, Announce and CancelAnnounce
*/

TorcSSDP::TorcSSDP()
  : QObject(),
    m_priv(new TorcSSDPPriv(this)),
    m_searchTimer(0),
    m_refreshTimer(0)
{
    gLocalContext->AddObserver(this);

    if (TorcNetwork::IsAvailable())
    {
        // kick off a full local search
        SearchPriv(QString(), NULL);

        // and schedule another search for after the MX time in our first request (3)
        m_searchTimer = startTimer(3000 + qrand() % 2000);
    }

    // minimum advertisement duration is 30 mins (1800 seconds). Check for expiry
    // every 5 minutes and flush stale entries
    m_refreshTimer = startTimer(5 * 60 * 1000);
}

TorcSSDP::~TorcSSDP()
{
    if (m_searchTimer)
        killTimer(m_searchTimer);

    if (m_refreshTimer)
        killTimer(m_refreshTimer);

    gLocalContext->RemoveObserver(this);
    delete m_priv;
}

/*! \fn    TorcSSDP::Search
 *  \brief Search for a specific UPnP device type
 *
 * Owner is notified (via a Torc::ServiceDiscovered event) about each discovered UPnP service or device
 * that matches Type (e.g urn:schemas-upnp-org:device:MediaServer:1). If the service is no longer available
 * or its announcement expires, Owner is sent a Torc::ServiceWentAway event.
 *
 * Searches initiated before the global TorcSSDP singleton has been created will be queued and
 * actioned when available.
 *
 * Call TorcSSDP::CancelSearch with matching arguments when notifications are no longer required.
*/
bool TorcSSDP::Search(const QString &Type, QObject *Owner)
{
    QMutexLocker locker(gSSDPLock);

    if (gSSDP)
    {
        QMetaObject::invokeMethod(gSSDP, "SearchPriv", Qt::AutoConnection, Q_ARG(QString, Type), Q_ARG(QObject*, Owner));
        return true;
    }
    else
    {
        gQueuedSearches.insert(Type, Owner);
    }

    return false;
}

/*! \fn    TorcSSDP::CancelSearch
 *  \brief Stop searching for a UPnP device type
 *
 * Owner will receive no more notifications about new or removed UPnP devices matching Type
*/
bool TorcSSDP::CancelSearch(const QString &Type, QObject *Owner)
{
    QMutexLocker locker(gSSDPLock);

    if (gSSDP)
    {
        QMetaObject::invokeMethod(gSSDP, "CancelSearchPriv", Qt::AutoConnection, Q_ARG(QString, Type), Q_ARG(QObject*, Owner));
        return true;
    }

    return false;
}

/*! \fn    TorcSSDP::Announce
 *  \brief Add a device to the list of services notified via SSDP
 *
 * The service described by Description will be announced via the SSDP mulitcast socket(s) and will be
 * re-announced approximately every 30 minutes (all devices/services are assumed to have an expiry of 1
 * hour).
*/
bool TorcSSDP::Announce(const TorcUPNPDescription &Description)
{
    QMutexLocker locker(gSSDPLock);

    if (gSSDP)
    {
        QMetaObject::invokeMethod(gSSDP, "AnnouncePriv", Qt::AutoConnection, Q_ARG(TorcUPNPDescription, Description));
        return true;
    }
    else
    {
        gQueuedAnnouncements << Description;
    }

    return false;
}

/*! \fn    TorcSSDP::CancelAnnounce
 *  \brief Cancel announcing a device via SSDP
*/
bool TorcSSDP::CancelAnnounce(const TorcUPNPDescription &Description)
{
    QMutexLocker locker(gSSDPLock);

    if (gSSDP)
    {
        QMetaObject::invokeMethod(gSSDP, "CancelAnnouncePriv", Qt::AutoConnection, Q_ARG(TorcUPNPDescription, Description));
        return true;
    }

    return false;
}

TorcSSDP* TorcSSDP::Create(bool Destroy)
{
    QMutexLocker locker(gSSDPLock);

    if (!Destroy)
    {
        if (!gSSDP)
            gSSDP = new TorcSSDP();
        return gSSDP;
    }

    delete gSSDP;
    gSSDP = NULL;
    return NULL;
}

void TorcSSDP::SearchPriv(const QString &Type, QObject *Owner)
{
    if (m_priv)
        m_priv->Search(Type, Owner);
}

void TorcSSDP::CancelSearchPriv(const QString &Type, QObject *Owner)
{
    if (m_priv)
        m_priv->CancelSearch(Type, Owner);
}

void TorcSSDP::AnnouncePriv(const TorcUPNPDescription Description)
{
    if (m_priv)
        m_priv->Announce(Description);
}

void TorcSSDP::CancelAnnouncePriv(const TorcUPNPDescription Description)
{
    if (m_priv)
        m_priv->CancelAnnounce(Description);
}

bool TorcSSDP::event(QEvent *Event)
{
    if (Event->type() == TorcEvent::TorcEventType)
    {
        TorcEvent* event = dynamic_cast<TorcEvent*>(Event);
        if (event)
        {
            if (event->GetEvent() == Torc::NetworkAvailable && m_priv)
            {
                m_priv->Start();
                SearchPriv(QString(), NULL);
                if (m_searchTimer)
                    killTimer(m_searchTimer);
                m_searchTimer = startTimer(3000 + qrand() % 2000);
            }
            else if (event->GetEvent() == Torc::NetworkUnavailable && m_priv)
            {
                if (m_searchTimer)
                    killTimer(m_searchTimer);
                m_searchTimer = 0;
                m_priv->Stop();
            }
        }
    }
    else if (Event->type() == QEvent::Timer)
    {
        QTimerEvent* event = dynamic_cast<QTimerEvent*>(Event);
        if (event)
        {
            if (event->timerId() == m_searchTimer)
            {
                killTimer(m_searchTimer);
                m_searchTimer = 0;
                SearchPriv(QString(), NULL);
            }
            else if (event->timerId() == m_refreshTimer && m_priv)
            {
                m_priv->Refresh();
            }
        }
    }

    return false;
}

void TorcSSDP::Read(void)
{
    if (m_priv)
    {
        QUdpSocket *socket = dynamic_cast<QUdpSocket*>(sender());
        m_priv->Read(socket);
    }
}

class TorcSSDPThread : public TorcQThread
{
  public:
    TorcSSDPThread() : TorcQThread("SSDP")
    {
    }

    void Start(void)
    {
        LOG(VB_GENERAL, LOG_INFO, "SSDP thread starting");
        TorcSSDP::Create();
    }

    void Finish(void)
    {
        TorcSSDP::Create(true/*destroy*/);
        LOG(VB_GENERAL, LOG_INFO, "SSDP thread stopping");
    }
};

class TorcSSDPObject : public TorcAdminObject
{
  public:
    TorcSSDPObject()
      : TorcAdminObject(TORC_ADMIN_MED_PRIORITY),
        m_ssdpThread(NULL)
    {
    }

    ~TorcSSDPObject()
    {
        Destroy();
    }

    void Create(void)
    {
        Destroy();

        m_ssdpThread = new TorcSSDPThread();
        m_ssdpThread->start();
    }

    void Destroy(void)
    {
        if (m_ssdpThread)
        {
            m_ssdpThread->quit();
            m_ssdpThread->wait();
        }

        delete m_ssdpThread;
        m_ssdpThread = NULL;
    }

  private:
    TorcSSDPThread *m_ssdpThread;

} TorcSSDPObject;
