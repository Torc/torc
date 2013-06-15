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
#include "torcthread.h"
#include "torcssdp.h"

class TorcSSDPPriv
{
  public:
    TorcSSDPPriv(TorcSSDP *Parent);
    ~TorcSSDPPriv();

    void         Start       (void);
    void         Stop        (void);
    void         Search      (const QString &Name);
    void         Announce    (const QString &Name, TorcSSDP::SSDPAnnounce Type);
    void         Read        (QUdpSocket *Socket);

  protected:
    TorcSSDP    *m_parent;
    bool         m_started;
    QHostAddress m_ipv4GroupAddress;
    QUdpSocket  *m_ipv4SearchSocket;
    QUdpSocket  *m_ipv4MulticastSocket;
    QString      m_ipv6LinkGroupBaseAddress;
    QHostAddress m_ipv6LinkGroupAddress;
    QUdpSocket  *m_ipv6LinkSearchSocket;
    QUdpSocket  *m_ipv6LinkMulticastSocket;
};

TorcSSDP* gSSDP = NULL;
QMutex*   gSSDPLock = new QMutex();

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
}

TorcSSDPPriv::~TorcSSDPPriv()
{
    Stop();
}

QUdpSocket* CreateSearchSocket(const QHostAddress &HostAddress, TorcSSDP *Parent)
{
    QUdpSocket *socket = new QUdpSocket();
    if (!socket->bind(HostAddress))
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to bind SSDP search socket (%1)").arg(socket->errorString()));
    socket->setSocketOption(QAbstractSocket::MulticastTtlOption, 4);
    socket->setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1);
    QObject::connect(socket, SIGNAL(readyRead()), Parent, SLOT(Read()));
    LOG(VB_NETWORK, LOG_INFO, "SSDP search socket " + socket->localAddress().toString() + ":" + QString::number(socket->localPort()));

    return socket;
}

QUdpSocket* CreateMulticastSocket(const QHostAddress &HostAddress, TorcSSDP *Parent, const QNetworkInterface &Interface)
{
    QUdpSocket *socket = new QUdpSocket();
    if (!socket->bind(HostAddress, 1900, QUdpSocket::ShareAddress))
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to bind SSDP multicase socket (%1)").arg(socket->errorString()));
    if (!socket->joinMulticastGroup(HostAddress, Interface))
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to join multicast group (%1)").arg(socket->errorString()));
    QObject::connect(socket, SIGNAL(readyRead()), Parent, SLOT(Read()));
    LOG(VB_NETWORK, LOG_INFO, "SSDP multicast socket " + socket->localAddress().toString() + ":" + QString::number(socket->localPort()));

    return socket;
}

void TorcSSDPPriv::Start(void)
{
    Stop();

    LOG(VB_GENERAL, LOG_INFO, "Starting SSDP discovery");

    // get the interface the network is using
    QNetworkInterface interface = TorcNetwork::GetInterface();
    m_ipv6LinkGroupAddress      = QHostAddress(m_ipv6LinkGroupBaseAddress + "%" + interface.name());
    m_ipv4SearchSocket          = CreateSearchSocket(QHostAddress("0.0.0.0"), m_parent);
    m_ipv4MulticastSocket       = CreateMulticastSocket(m_ipv4GroupAddress, m_parent, interface);
    m_ipv6LinkSearchSocket      = CreateSearchSocket(QHostAddress("::"), m_parent);
    m_ipv6LinkMulticastSocket   = CreateMulticastSocket(m_ipv6LinkGroupAddress, m_parent, interface);

    // and search
    QByteArray search("M-SEARCH * HTTP/1.1\r\n"
                      "HOST: 239.255.255.250:1900\r\n"
                      "MAN: \"ssdp:discover\"\r\n"
                      "MX: 3\r\n"
                      "ST: ssdp:all\r\n\r\n");

    qint64 sent = m_ipv4SearchSocket->writeDatagram(search, m_ipv4GroupAddress, 1900);
    if (sent != search.size())
        LOG(VB_GENERAL, LOG_ERR, QString("Error sending search request (%1)").arg(m_ipv4SearchSocket->errorString()));

    sent = m_ipv6LinkSearchSocket->writeDatagram(search, m_ipv6LinkGroupAddress, 1900);
    if (sent != search.size())
        LOG(VB_GENERAL, LOG_ERR, QString("Error sending search request (%1)").arg(m_ipv6LinkSearchSocket->errorString()));

    LOG(VB_NETWORK, LOG_INFO, "Sent SSDP search requests");
    m_started = true;
}

void TorcSSDPPriv::Stop(void)
{
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

void TorcSSDPPriv::Search(const QString &Name)
{
    LOG(VB_GENERAL, LOG_INFO, "SEARCH");
}

void TorcSSDPPriv::Announce(const QString &Name, TorcSSDP::SSDPAnnounce Type)
{
    LOG(VB_GENERAL, LOG_INFO, "ANNOUNCE");
}

void TorcSSDPPriv::Read(QUdpSocket *Socket)
{
    while (Socket && Socket->hasPendingDatagrams())
    {
        QByteArray datagram;
        datagram.resize(Socket->pendingDatagramSize());
        Socket->readDatagram(datagram.data(), datagram.size());
        LOG(VB_NETWORK, LOG_DEBUG, datagram);
    }
}

TorcSSDP::TorcSSDP()
  : QObject(),
    m_priv(new TorcSSDPPriv(this))
{
    gLocalContext->AddObserver(this);
}

TorcSSDP::~TorcSSDP()
{
    gLocalContext->RemoveObserver(this);
    delete m_priv;
}

bool TorcSSDP::Search(const QString &Name)
{
    QMutexLocker locker(gSSDPLock);

    if (gSSDP)
    {
        QMetaObject::invokeMethod(gSSDP, "SearchPriv", Qt::AutoConnection, Q_ARG(QString, Name));
        return true;
    }

    return false;
}

bool TorcSSDP::Announce(const QString &Name, SSDPAnnounce Type)
{
    QMutexLocker locker(gSSDPLock);

    if (gSSDP)
    {
        QMetaObject::invokeMethod(gSSDP, "AnnouncePriv", Qt::AutoConnection, Q_ARG(QString, Name), Q_ARG(SSDPAnnounce, Type));
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
    return NULL;
}

void TorcSSDP::SearchPriv(const QString &Name)
{
    if (m_priv)
        m_priv->Search(Name);
}


void TorcSSDP::AnnouncePriv(const QString &Name, SSDPAnnounce Type)
{
    if (m_priv)
        m_priv->Announce(Name, Type);
}

bool TorcSSDP::event(QEvent *Event)
{
    if (Event->type() == TorcEvent::TorcEventType)
    {
        TorcEvent* event = dynamic_cast<TorcEvent*>(Event);
        if (event)
        {
            if (event->Event() == Torc::NetworkAvailable && m_priv)
                m_priv->Start();
            else if (event->Event() == Torc::NetworkUnavailable && m_priv)
                m_priv->Stop();
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

class TorcSSDPThread : public TorcThread
{
  public:
    TorcSSDPThread()
      : TorcThread("SSDP")
    {
    }

    void run(void)
    {
        RunProlog();
        LOG(VB_GENERAL, LOG_INFO, "SSDP thread starting");

        TorcSSDP::Create();

        // run the event loop
        exec();

        TorcSSDP::Create(true/*destroy*/);

        LOG(VB_GENERAL, LOG_INFO, "SSDP thread stopping");
        RunEpilog();
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
