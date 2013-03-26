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

void TorcRAOPDevice::Enable(bool Enable)
{
    bool allow = gLocalContext->GetFlag(Torc::Server) &&
                 gLocalContext->GetSetting(TORC_AUDIO + "RAOPEnabled", false) &&
                 TorcRAOPConnection::LoadKey();

    {
        QMutexLocker locker(gTorcRAOPLock);

        if (gTorcRAOPDevice)
        {
            if (Enable && allow)
                return;

            delete gTorcRAOPDevice;
            gTorcRAOPDevice = NULL;
            return;
        }

        if (Enable && allow)
        {
            gTorcRAOPDevice = new TorcRAOPDevice();
            return;
        }

        delete gTorcRAOPDevice;
        gTorcRAOPDevice = NULL;
    }
}

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
    m_port(0),
    m_macAddress(DEFAULT_MAC_ADDRESS),
    m_bonjourReference(0),
    m_lock(new QMutex(QMutex::Recursive))
{
    connect(this, SIGNAL(newConnection()), this, SLOT(NewConnection()));
    gLocalContext->AddObserver(this);

    int lastport = gLocalContext->GetSetting(TORC_AUDIO + "RAOPServerPort", 4850);
    m_port = lastport > 1023 ? lastport : 4850;

    // no network = no mac address (and no clients)
    if (TorcNetwork::IsAvailable())
        Open();
    else
        LOG(VB_GENERAL, LOG_INFO, "Deferring RAOP server startup until network available");
}

TorcRAOPDevice::~TorcRAOPDevice()
{
    Close();

    gLocalContext->RemoveObserver(this);

    delete m_lock;
    m_lock = NULL;
}

bool TorcRAOPDevice::Open(void)
{
    QMutexLocker locker(m_lock);

    if (!isListening())
    {
        // start listening for connections
        if (!listen(QHostAddress::Any, m_port))
            if (m_port > 0)
                listen();
    }

    if (!isListening())
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to open RAOP port");
        Close();
        return false;
    }

    // try and use the same port across power events
    int newport = serverPort();

    if (newport != m_port)
    {
        m_port = serverPort();
        gLocalContext->SetSetting(TORC_AUDIO + "RAOPServerPort", m_port);
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
        QByteArray txt;
        txt.append(6); txt.append("tp=UDP");
        txt.append(8); txt.append("sm=false");
        txt.append(8); txt.append("sv=false");
        txt.append(4); txt.append("ek=1");
        txt.append(6); txt.append("et=0,1");
        txt.append(6); txt.append("cn=0,1");
        txt.append(4); txt.append("ch=2");
        txt.append(5); txt.append("ss=16");
        txt.append(8); txt.append("sr=44100");
        txt.append(8); txt.append("pw=false");
        txt.append(4); txt.append("vn=3");
        txt.append(9); txt.append("txtvers=1");
        txt.append(8); txt.append("md=0,1,2");
        txt.append(9); txt.append("vs=130.14");
        txt.append(7); txt.append("da=true");

        m_bonjourReference = TorcBonjour::Instance()->Register(m_port, type, name, txt);
        if (!m_bonjourReference)
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to advertise RAOP device");
            Close();
            return false;
        }
    }

    LOG(VB_GENERAL, LOG_INFO, QString("RAOP server listening on port %1").arg(m_port));
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
            LOG(VB_GENERAL, LOG_INFO, QString("New RAOP connection from %1:%2")
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
            LOG(VB_GENERAL, LOG_INFO, QString("Removing client connected from %1:%2")
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
            int event = torcevent->Event();

            if (event == Torc::NetworkAvailable && !isListening())
            {
                if (TorcNetwork::IsAllowed())
                    Open();
            }
            else if (event == Torc::NetworkEnabled && !isListening())
            {
                // need network for Mac Address - unless it's already been started once
                if (TorcNetwork::IsAvailable() || m_macAddress != DEFAULT_MAC_ADDRESS)
                    Open();
            }
            else if (event == Torc::NetworkDisabled && isListening())
            {
                Close(true);
            }
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

class TorcRAOPObject : public TorcAdminObject
{
  public:
    TorcRAOPObject()
      : TorcAdminObject(TORC_ADMIN_LOW_PRIORITY)
    {
    }

    void Create(void)
    {
        TorcRAOPDevice::Enable(true);
    }

    void Destroy(void)
    {
        TorcRAOPDevice::Enable(false);
    }

} TorcRAOPObject;
