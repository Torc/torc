/* Class TorcRAOPDevice
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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

// Qt
#include <QNetworkInterface>
#include <QTcpSocket>
#include <QMutex>

// Torc
#include "torclocalcontext.h"
#include "torclogging.h"
#include "torcbonjour.h"
#include "torcadminthread.h"
#include "torcnetwork.h"
#include "torcraopconnection.h"
#include "torcraopdevice.h"

/*! \class TorcRAOPDevice
 *  \brief A Remote Audio Output Server
 *
 * TorcRAOPDevice is a sub-class of QTcpServer that listens for incoming
 * RAOP connections. The port can be configured and will default to 4850.
 * If the requested port is not available, it will request any available port
 * and the port setting will be updated to ensure a consistent port between
 * sessions. The service is then advertised via Bonjour.
 *
 * \sa TorcRAOPBuffer
 * \sa TorcRAOPConnection
*/

QMutex* TorcRAOPDevice::gTorcRAOPLock = new QMutex();
TorcRAOPDevice* TorcRAOPDevice::gTorcRAOPDevice = NULL;

QByteArray* TorcRAOPDevice::Read(int Reference)
{
    if (Reference < 1)
        return NULL;

    QMutexLocker locker(gTorcRAOPLock);

    if (gTorcRAOPDevice)
        return gTorcRAOPDevice->ReadPacket(Reference);
    return NULL;
}

TorcRAOPDevice::TorcRAOPDevice()
  : QTcpServer(NULL),
    m_enabled(NULL),
    m_port(NULL),
    m_macAddress(DEFAULT_MAC_ADDRESS),
    m_bonjourReference(0),
    m_lock(new QMutex(QMutex::Recursive))
{
    // main setting
    TorcSetting *parent = gRootSetting->FindChild(SETTING_NETWORKALLOWEDINBOUND, true);

    if (parent)
    {
        m_enabled = new TorcSetting(parent, QString(TORC_AUDIO + "RAOPEnabled"), tr("Enable AirTunes playback"),
                                    TorcSetting::Checkbox, true, QVariant((bool)true));
        m_enabled->SetActiveThreshold(2);
        if (parent->IsActive())
            m_enabled->SetActive(true);
        if (parent->GetValue().toBool())
            m_enabled->SetActive(true);
        connect(parent,    SIGNAL(ValueChanged(bool)),      m_enabled, SLOT(SetActive(bool)));
        connect(parent,    SIGNAL(ActivationChanged(bool)), m_enabled, SLOT(SetActive(bool)));
        connect(m_enabled, SIGNAL(ValueChanged(bool)),      this,      SLOT(Enable(bool)));
        connect(m_enabled, SIGNAL(ActivationChanged(bool)), this,      SLOT(Enable(bool)));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to find inbound network setting");
    }

    // port setting (not visible to end user)
    m_port = new TorcSetting(NULL, QString(TORC_AUDIO + "RAOPServerPort"), QString(), TorcSetting::Integer, true, QVariant((int)4850));

    // listen for connections
    connect(this, SIGNAL(newConnection()), this, SLOT(NewConnection()));

    // listen for events
    gLocalContext->AddObserver(this);

    // no network = no mac address (and no clients)
    if (TorcNetwork::IsAvailable())
        Enable(m_enabled->GetValue().toBool() && m_enabled->IsActive());
    else
        LOG(VB_GENERAL, LOG_INFO, "Deferring RAOP server startup");
}

TorcRAOPDevice::~TorcRAOPDevice()
{
    Close();

    if (m_port)
    {
        m_port->Remove();
        m_port->DownRef();
        m_port = NULL;
    }

    if (m_enabled)
    {
        m_enabled->Remove();
        m_enabled->DownRef();
        m_enabled = NULL;
    }

    gLocalContext->RemoveObserver(this);

    delete m_lock;
    m_lock = NULL;
}

bool TorcRAOPDevice::Open(void)
{
    QMutexLocker locker(m_lock);

    if (!TorcRAOPConnection::LoadKey())
        return false;

    int port = m_port->GetValue().toInt();

    bool waslistening = isListening();

    if (!waslistening)
    {
        // QHostAddress::Any will listen to all IPv4 and IPv6 addresses on Qt 5.x but to IPv4
        // addresses only on Qt 4.8. So try IPv6 first on Qt 4.8 and fall back to any.
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
        if (!listen(QHostAddress::AnyIPv6, port))
            if (serverError() == QAbstractSocket::UnsupportedSocketOperationError)
                LOG(VB_GENERAL, LOG_INFO, "IPv6 not available");
#endif
        if (!isListening() && !listen(QHostAddress::Any, port))
            if (port > 0)
                listen();
    }

    if (!isListening())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to open RAOP port (%1)").arg(errorString()));
        Close();
        return false;
    }

    // try and use the same port across power events
    int newport = serverPort();
    if (newport != port)
    {
        m_port->SetValue(QVariant((int)newport));
        port = newport;

        // re-advertise if port changed
        if (m_bonjourReference)
        {
            TorcBonjour::Instance()->Deregister(m_bonjourReference);
            m_bonjourReference = 0;
        }
    }

    // advertise
    if (!m_bonjourReference)
    {
        m_macAddress = TorcNetwork::GetMACAddress();
        QByteArray name = m_macAddress.remove(':').toLatin1();
        name.append("@");
        name.append("Torc");
        name.append(" on ");
        name.append(QHostInfo::localHostName());
        QByteArray type = "_raop._tcp";
        QMap<QByteArray,QByteArray> txt;
        txt.insert("tp",      "UDP");
        txt.insert("sm",      "false");
        txt.insert("sv",      "false");
        txt.insert("ek",      "1");
        txt.insert("et",      "0,1");
        txt.insert("cn",      "0,1");
        txt.insert("ch",      "2");
        txt.insert("ss",      "16");
        txt.insert("sr",      "44100");
        txt.insert("pw",      "false");
        txt.insert("vn",      "3");
        txt.insert("txtvers", "1");
        txt.insert("md",      "0,1,2");
        txt.insert("vs",      "130.14");
        txt.insert("da",      "true");

        m_bonjourReference = TorcBonjour::Instance()->Register(port, type, name, txt);
        if (!m_bonjourReference)
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to advertise RAOP device");
            Close();
            return false;
        }
    }

    if (!waslistening)
        LOG(VB_GENERAL, LOG_INFO, QString("RAOP server listening on port %1").arg(port));
    return true;
}

void TorcRAOPDevice::Close(bool Suspend)
{
    QMutexLocker locker(m_lock);

    // stop server
    bool running = isListening();
    close();

    // close outstanding connections
    QMap<int,TorcRAOPConnection*>::iterator it = m_connections.begin();
    for ( ; it != m_connections.end(); ++it)
        (*it)->deleteLater();
    m_connections.clear();

    if (!Suspend)
    {
        // stop advertising only when actually closing
        if (m_bonjourReference)
            TorcBonjour::Instance()->Deregister(m_bonjourReference);
        m_bonjourReference = 0;
    }

    if (running)
        LOG(VB_GENERAL, LOG_INFO, QString("RAOP server %1").arg(Suspend ? "suspended" : "closed"));
}

void TorcRAOPDevice::NewConnection(void)
{
    while (hasPendingConnections())
    {
        QTcpSocket *client = nextPendingConnection();

        if (client)
        {
            LOG(VB_GENERAL, LOG_INFO, QString("New RAOP connection from %1 on port %2")
                .arg(client->peerAddress().toString()).arg(client->peerPort()));

            static int reference = 0;
            {
                QMutexLocker locker(m_lock);
                if (++reference < 1)
                    reference = 1;
            }

            TorcRAOPConnection *connection = new TorcRAOPConnection(client, reference, m_macAddress);
            if (!connection->Open())
            {
                connection->deleteLater();
                client->disconnectFromHost();
                delete client;
            }

            connect(client, SIGNAL(disconnected()), this, SLOT(DeleteConnection()));
            {
                QMutexLocker locker(m_lock);
                m_connections.insert(reference, connection);
            }
        }
    }
}

void TorcRAOPDevice::DeleteConnection(void)
{
    QMutexLocker locker(m_lock);

    QMap<int,TorcRAOPConnection*>::iterator it = m_connections.begin();
    for ( ; it != m_connections.end(); ++it)
    {
        QTcpSocket* socket = it.value()->MasterSocket();
        if (socket->state() == QTcpSocket::UnconnectedState)
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Removing client connected from %1 on port %2")
                .arg(socket->peerAddress().toString()).arg(socket->peerPort()));
            it.value()->deleteLater();
            m_connections.remove(it.key());
            return;
        }
    }
}

bool TorcRAOPDevice::event(QEvent *Event)
{
    if (Event->type() == TorcEvent::TorcEventType)
    {
        TorcEvent* torcevent = dynamic_cast<TorcEvent*>(Event);
        if (torcevent)
        {
            if (torcevent->GetEvent() == Torc::NetworkAvailable)
                Enable(true);
        }
    }

    return false;
}

QByteArray* TorcRAOPDevice::ReadPacket(int Reference)
{
    QMutexLocker locker(m_lock);

    QMap<int,TorcRAOPConnection*>::iterator it = m_connections.find(Reference);
    if (it != m_connections.end())
        return it.value()->Read();

    return NULL;
}

void TorcRAOPDevice::Enable(bool Enable)
{
    if (TorcNetwork::IsAvailable() && Enable && m_enabled->IsActive() && m_enabled->GetValue().toBool())
        Open();
    else
        Close();
}

class TorcRAOPObject : public TorcAdminObject
{
  public:
    TorcRAOPObject()
      : TorcAdminObject(TORC_ADMIN_LOW_PRIORITY)
    {
    }

    void Create(void)
    {
        Destroy();

        {
            QMutexLocker locker(TorcRAOPDevice::gTorcRAOPLock);
            if (gLocalContext->FlagIsSet(Torc::Server))
                TorcRAOPDevice::gTorcRAOPDevice = new TorcRAOPDevice();
        }
    }

    void Destroy(void)
    {
        QMutexLocker locker(TorcRAOPDevice::gTorcRAOPLock);
        if (TorcRAOPDevice::gTorcRAOPDevice)
        {
            delete TorcRAOPDevice::gTorcRAOPDevice;
            TorcRAOPDevice::gTorcRAOPDevice = NULL;
        }
    }

} TorcRAOPObject;
