/* Class TorcHTTPServer
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

// Qt
#include <QUrl>
#include <QTcpSocket>
#include <QCoreApplication>

// Torc
#include "torccompat.h"
#include "torclocalcontext.h"
#include "torclogging.h"
#include "torcadminthread.h"
#include "torcnetwork.h"
#include "torcbonjour.h"
#include "torchttpconnection.h"
#include "torchttprequest.h"
#include "torchtmlhandler.h"
#include "torchttphandler.h"
#include "torchttpservice.h"
#include "torchttpserver.h"

#ifndef Q_OS_WIN
#include <sys/utsname.h>
#endif

/*! \class TorcHTTPServer
 *  \brief An HTTP server.
 *
 * TorcHTTPServer listens for incoming TCP connections. The default port is 4840
 * though any available port may be used if the default is unavailable. New
 * connections are passed to instances of TorcHTTPConnection.
 *
 * TorcHTTPServer maintains a list of TorcHTTPHandlers that will process valid
 * incoming HTTP requests. Register new handlers with RegisterHandler and remove
 * them with DeregisterHandler.
 *
 * \sa TorcHTTPRequest
 * \sa TorcHTTPHandler
 * \sa TorcHTTPConnection
 *
 * \todo Entirely single threaded
*/

TorcHTTPServer* TorcHTTPServer::gWebServer = NULL;
QMutex*         TorcHTTPServer::gWebServerLock = new QMutex(QMutex::Recursive);
QString         TorcHTTPServer::gPlatform = QString("");

void TorcHTTPServer::RegisterHandler(TorcHTTPHandler *Handler)
{
    QMutexLocker locker(gWebServerLock);

    static QList<TorcHTTPHandler*> handlerqueue;

    if (gWebServer)
    {
        while (!handlerqueue.isEmpty())
            gWebServer->AddHandler(handlerqueue.takeFirst());
        gWebServer->AddHandler(Handler);
    }
    else
    {
        handlerqueue.append(Handler);
    }
}

void TorcHTTPServer::DeregisterHandler(TorcHTTPHandler *Handler)
{
    QMutexLocker locker(gWebServerLock);

    if (gWebServer)
        gWebServer->RemoveHandler(Handler);
}

int TorcHTTPServer::GetPort(void)
{
    QMutexLocker locker(gWebServerLock);

    if (gWebServer)
        return gWebServer->serverPort();

    return 0;
}

bool TorcHTTPServer::IsListening(void)
{
    QMutexLocker locker(gWebServerLock);

    if (gWebServer)
        return gWebServer->isListening();

    return false;
}

QString TorcHTTPServer::PlatformName(void)
{
    return gPlatform;
}

TorcHTTPServer::TorcHTTPServer()
  : QTcpServer(),
    m_enabled(NULL),
    m_port(NULL),
    m_defaultHandler(NULL),
    m_servicesHelpHandler(NULL),
    m_servicesDirectory(SERVICES_DIRECTORY),
    m_handlersLock(new QMutex(QMutex::Recursive)),
    m_newHandlersLock(new QMutex(QMutex::Recursive)),
    m_oldHandlersLock(new QMutex(QMutex::Recursive)),
    m_httpBonjourReference(0),
    m_torcBonjourReference(0)
{
    // ensure service directory is valid
    if (!m_servicesDirectory.endsWith("/"))
        m_servicesDirectory += "/";

    // create main setting
    TorcSetting *parent = gRootSetting->FindChild(SETTING_NETWORKALLOWEDINBOUND, true);
    if (parent)
    {
        m_enabled = new TorcSetting(parent, SETTING_WEBSERVERENABLED, tr("Enable internal web server"),
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

    // port setting - this could become a user editable setting
    m_port = new TorcSetting(NULL, TORC_CORE + "WebServerPort", QString(), TorcSetting::Integer, true, QVariant((int)4840));

    // listen for new handlers
    connect(this, SIGNAL(HandlersChanged()), this, SLOT(UpdateHandlers()));

    static bool initialised = false;
    if (!initialised)
    {
        initialised = true;
#ifdef Q_OS_WIN
        gPlatform = QString("Windows %1.%2").arg(LOBYTE(LOWORD(GetVersion()))).arg(HIBYTE(LOWORD(GetVersion())));
#else
        struct utsname info;
        uname(&info);
        gPlatform = QString("%1 %2").arg(info.sysname).arg(info.release);
#endif
    }

    // add the default top level handler
    m_defaultHandler = new TorcHTMLHandler("", QCoreApplication::applicationName());
    AddHandler(m_defaultHandler);

    // services help
    m_servicesHelpHandler = new TorcHTMLServicesHelp(SERVICES_DIRECTORY, tr("Services"));
    AddHandler(m_servicesHelpHandler);

    // and start
    // NB this will start and stop purely on the basis of the setting, irrespective
    // of network availability and hence is still available via 'localhost'
    Enable(true);
}

TorcHTTPServer::~TorcHTTPServer()
{
    delete m_defaultHandler;
    delete m_servicesHelpHandler;

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

    delete m_handlersLock;
    delete m_newHandlersLock;
    delete m_oldHandlersLock;
}

void TorcHTTPServer::Enable(bool Enable)
{
    if (Enable && m_enabled && m_enabled->IsActive() && m_enabled->GetValue().toBool())// && TorcNetwork::IsAvailable())
        Open();
    else
        Close();
}

bool TorcHTTPServer::Open(void)
{
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
        LOG(VB_GENERAL, LOG_ERR, "Failed to open web server port");
        Close();
        return false;
    }

    // try to use the same port
    if (port != serverPort())
    {
        port = serverPort();
        m_port->SetValue(QVariant((int)port));

        // re-advertise if the port has changed
        if (m_httpBonjourReference)
        {
            TorcBonjour::Instance()->Deregister(m_httpBonjourReference);
            m_httpBonjourReference = 0;
        }

        if (m_torcBonjourReference)
        {
            TorcBonjour::Instance()->Deregister(m_torcBonjourReference);
            m_torcBonjourReference = 0;
        }
    }

    // advertise service if not already doing so
    if (!m_httpBonjourReference || !m_torcBonjourReference)
    {
        QByteArray dummy;
        QByteArray name(QCoreApplication::applicationName().toLatin1());
        name.append(" on ");
        name.append(QHostInfo::localHostName());

        if (!m_httpBonjourReference)
            m_httpBonjourReference = TorcBonjour::Instance()->Register(port, "_http._tcp.", name, dummy);

        if (!m_torcBonjourReference)
            m_torcBonjourReference = TorcBonjour::Instance()->Register(port, "_torc._tcp", name, dummy);
    }

    if (!waslistening)
        LOG(VB_GENERAL, LOG_INFO, QString("Web server listening on port %1").arg(port));
    return true;
}

void TorcHTTPServer::Close(void)
{
    // stop advertising
    if (m_httpBonjourReference)
    {
        TorcBonjour::Instance()->Deregister(m_httpBonjourReference);
        m_httpBonjourReference = 0;
    }

    if (m_torcBonjourReference)
    {
        TorcBonjour::Instance()->Deregister(m_torcBonjourReference);
        m_torcBonjourReference = 0;
    }

    // actually close
    close();

    // remove any currect connections
    while (!m_connections.isEmpty())
    {
        QMap<QTcpSocket*,TorcHTTPConnection*>::iterator it = m_connections.begin();
        TorcHTTPConnection* connection = it.value();
        m_connections.erase(it);
        connection->DownRef();
    }

    LOG(VB_GENERAL, LOG_INFO, "Webserver closed");
}

void TorcHTTPServer::HandleRequest(TorcHTTPConnection *Connection, TorcHTTPRequest *Request)
{
    if (Request && Connection)
    {
        m_handlersLock->lock();
        QMap<QString,TorcHTTPHandler*>::iterator it = m_handlers.find(Request->GetPath());
        if (it != m_handlers.end())
            (*it)->ProcessHTTPRequest(this, Request, Connection);
        m_handlersLock->unlock();
    }
}

bool TorcHTTPServer::event(QEvent *Event)
{
    if (Event->type() == TorcEvent::TorcEventType)
    {
        TorcEvent* torcevent = dynamic_cast<TorcEvent*>(Event);
        if (torcevent)
        {
            if (torcevent->Event() == Torc::NetworkAvailable)
                Enable(true);
        }
    }

    return QTcpServer::event(Event);
}

void TorcHTTPServer::incomingConnection(qintptr SocketDescriptor)
{
    QTcpSocket *socket = new QTcpSocket();
    if (!socket->setSocketDescriptor(SocketDescriptor))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to set socket descriptor for new socket connection");
        return;
    }

    TorcHTTPConnection *connection = new TorcHTTPConnection(this, socket);
    m_connections.insert(socket, connection);
}

void TorcHTTPServer::ClientDisconnected(void)
{
    QTcpSocket *socket = (QTcpSocket*)sender();

    if (m_connections.contains(socket))
    {
        TorcHTTPConnection* connection = m_connections.take(socket);
        connection->DownRef();
    }
}

QMap<QString,QString> TorcHTTPServer::GetServiceHandlers(void)
{
    QMap<QString,QString> result;

    m_handlersLock->lock();
    QMap<QString,TorcHTTPHandler*>::const_iterator it = m_handlers.begin();
    for ( ; it != m_handlers.end(); ++it)
        if (it.key().startsWith(m_servicesDirectory))
            result.insert(it.key(), it.value()->Name());
    m_handlersLock->unlock();

    return result;
}

void TorcHTTPServer::AddHandler(TorcHTTPHandler *Handler)
{
    if (!Handler)
        return;

    {
        QMutexLocker locker(m_newHandlersLock);
        m_newHandlers.append(Handler);
    }

    emit HandlersChanged();
}

void TorcHTTPServer::RemoveHandler(TorcHTTPHandler *Handler)
{
    if (!Handler)
        return;

    {
        QMutexLocker locker(m_oldHandlersLock);
        m_oldHandlers.append(Handler);
    }

    emit HandlersChanged();
}

void TorcHTTPServer::UpdateHandlers(void)
{
    QList<TorcHTTPHandler*> newhandlers;
    QList<TorcHTTPHandler*> oldhandlers;

    {
        QMutexLocker locker(m_newHandlersLock);
        newhandlers = m_newHandlers;
        m_newHandlers.clear();
    }

    {
        QMutexLocker locker(m_oldHandlersLock);
        oldhandlers = m_oldHandlers;
        m_oldHandlers.clear();
    }

    {
        QMutexLocker locker(m_handlersLock);

        foreach (TorcHTTPHandler *handler, newhandlers)
        {
            QString signature = handler->Signature();
            if (m_handlers.contains(signature))
            {
                LOG(VB_GENERAL, LOG_WARNING, QString("Handler '%1' already registered - ignoring").arg(signature));
            }
            else if (!signature.isEmpty())
            {
                LOG(VB_GENERAL, LOG_DEBUG, QString("Added handler '%1' for %2").arg(handler->Name()).arg(signature));
                m_handlers.insert(signature, handler);
            }
        }

        foreach (TorcHTTPHandler *handler, oldhandlers)
        {
            QMap<QString,TorcHTTPHandler*>::iterator it = m_handlers.begin();
            for ( ; it != m_handlers.end(); ++it)
            {
                if (it.value() == handler)
                {
                    m_handlers.erase(it);
                    break;
                }
            }
        }
    }
}

class TorcHTTPServerObject : public TorcAdminObject
{
  public:
    TorcHTTPServerObject()
      : TorcAdminObject(TORC_ADMIN_HIGH_PRIORITY)
    {
    }

    void Create(void)
    {
        Destroy();

        QMutexLocker locker(TorcHTTPServer::gWebServerLock);
        if (gLocalContext->FlagIsSet(Torc::Server))
            TorcHTTPServer::gWebServer = new TorcHTTPServer();
    }

    void Destroy(void)
    {
        QMutexLocker locker(TorcHTTPServer::gWebServerLock);

        // this may need to use deleteLater but this causes issues with setting deletion
        // as the object is actually destroyed after the parent (network) setting
        if (TorcHTTPServer::gWebServer)
            delete TorcHTTPServer::gWebServer;

        TorcHTTPServer::gWebServer = NULL;
    }
} TorcHTTPServerObject;


