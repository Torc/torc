/* Class TorcHTTPServer
*
* This file is part of the Torc project.
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
 * \todo Move UserServicesHelp elsewhere
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

void TorcHTTPServer::Create(void)
{
    QMutexLocker locker(gWebServerLock);

    if (gWebServer)
        return;

    gWebServer = new TorcHTTPServer();
    if (gWebServer->Open())
        return;

    delete gWebServer;
    gWebServer = NULL;
}

void TorcHTTPServer::Destroy(void)
{
    QMutexLocker locker(gWebServerLock);

    if (gWebServer)
    {
        gWebServer->Close();
        gWebServer->deleteLater();
    }

    gWebServer = NULL;
}

TorcHTTPServer::TorcHTTPServer()
  : QTcpServer(),
    m_defaultHandler(NULL),
    m_servicesDirectory(SERVICES_DIRECTORY),
    m_port(0),
    m_newHandlersLock(new QMutex(QMutex::Recursive)),
    m_oldHandlersLock(new QMutex(QMutex::Recursive))
{
    if (!m_servicesDirectory.endsWith("/"))
        m_servicesDirectory += "/";

    m_port = gLocalContext->GetSetting(TORC_CORE + "WebServerPort", 4840);
    connect(this, SIGNAL(newConnection()), this, SLOT(ClientConnected()));
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
}

TorcHTTPServer::~TorcHTTPServer()
{
    delete m_defaultHandler;

    Close();

    delete m_newHandlersLock;
    delete m_oldHandlersLock;
}

bool TorcHTTPServer::Open(void)
{
    if (!isListening())
        if (!listen(QHostAddress::Any, m_port))
            listen();

    if (!isListening())
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to open web server port");
        Close();
        return false;
    }

    if (m_port != serverPort())
    {
        m_port = serverPort();
        gLocalContext->SetSetting(TORC_CORE + "WebServerPort", m_port);
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Web server listening on port %1").arg(m_port));
    return true;
}

void TorcHTTPServer::Close(void)
{
    close();

    while (!m_connections.isEmpty())
    {
        QMap<QTcpSocket*,TorcHTTPConnection*>::iterator it = m_connections.begin();
        TorcHTTPConnection* connection = it.value();
        m_connections.erase(it);
        connection->deleteLater();
    }

    LOG(VB_GENERAL, LOG_INFO, "Webserver closed");
}

void TorcHTTPServer::NewRequest(void)
{
    TorcHTTPConnection* connection = static_cast<TorcHTTPConnection*>(sender());

    while (connection && connection->HasRequests())
    {
        TorcHTTPRequest* request = connection->GetRequest();
        if (request)
        {
            if (request->GetHTTPType() == HTTPResponse)
            {
                LOG(VB_GENERAL, LOG_INFO, "Received HTTP response...");
                delete request;
            }
            else
            {
                if (request->GetPath() == m_servicesDirectory)
                {
                    // top level services help...
                    UserServicesHelp(request, connection);
                }
                else
                {
                    QMap<QString,TorcHTTPHandler*>::iterator it = m_handlers.find(request->GetPath());
                    if (it != m_handlers.end())
                        (*it)->ProcessHTTPRequest(this, request, connection);
                }
                connection->Complete(request);
            }
        }
    }
}

bool TorcHTTPServer::event(QEvent *Event)
{
    return QTcpServer::event(Event);
}

void TorcHTTPServer::ClientConnected(void)
{
    while (hasPendingConnections())
    {
        QTcpSocket *socket = nextPendingConnection();
        TorcHTTPConnection *connection = new TorcHTTPConnection(this, socket);
        m_connections.insert(socket, connection);
    }
}

void TorcHTTPServer::ClientDisconnected(void)
{
    QTcpSocket *socket = (QTcpSocket*)sender();

    if (m_connections.contains(socket))
    {
        TorcHTTPConnection* connection = m_connections.take(socket);
        connection->deleteLater();
    }
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

void TorcHTTPServer::UserServicesHelp(TorcHTTPRequest *Request, TorcHTTPConnection *Connection)
{
    QByteArray *result = new QByteArray(1024, 0);
    QTextStream stream(result);

    QMap<QString,QString> services;
    QList<QString> names;
    QMap<QString,TorcHTTPHandler*>::const_iterator it = m_handlers.begin();
    for ( ; it != m_handlers.end(); ++it)
    {
        if (it.key().startsWith(m_servicesDirectory))
        {
            services.insert(it.key(), it.key() + "help");
            names.append(it.value()->Name());
        }
    }

    stream << "<html><head><title>" << QCoreApplication::applicationName() << "</title></head>";
    stream << "<body><h1><a href='/'>" << QCoreApplication::applicationName();
    stream << "<a> " << tr("Services") << "</a></h1>";

    if (services.isEmpty())
    {
        stream << "<h3>" << tr("No services are registered") << "</h3>";
    }
    else
    {
        stream << "<h3>" << tr("Available services") << "</h3>";
        QMap<QString,QString>::iterator it = services.begin();
        QList<QString>::iterator name = names.begin();
        for ( ; it != services.end(); ++it, ++name)
            stream << (*name) << " <a href='" << it.value() << "'>" << it.key() << "</a><br>";
    }

    stream << "</body></html>";
    stream.flush();
    Request->SetStatus(HTTP_OK);
    Request->SetResponseType(HTTPResponseHTML);
    Request->SetResponseContent(result);
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
        TorcHTTPServer::Create();
    }

    void Destroy(void)
    {
        TorcHTTPServer::Destroy();
    }
} TorcHTTPServerObject;


