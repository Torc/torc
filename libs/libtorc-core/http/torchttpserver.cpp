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
#include "version.h"
#include "torccompat.h"
#include "torclocalcontext.h"
#include "torclogging.h"
#include "torccoreutils.h"
#include "torcadminthread.h"
#include "torcnetwork.h"
#include "torchttpconnection.h"
#include "torchttprequest.h"
#include "torchtmlhandler.h"
#include "torchttphandler.h"
#include "torchttpservice.h"
#include "torchttpserver.h"

#if defined(CONFIG_LIBDNS_SD) && CONFIG_LIBDNS_SD
#include "torcbonjour.h"
#endif

#ifndef Q_OS_WIN
#include <sys/utsname.h>
#endif

///\brief A simple container for authenticaion tokens.
class WebSocketAuthentication
{
  public:
    WebSocketAuthentication() { }
    WebSocketAuthentication(quint64 Timestamp, const QString &UserName, const QString &Host)
      : m_timeStamp(Timestamp),
        m_userName(UserName),
        m_host(Host)
    {
    }

    quint64 m_timeStamp;
    QString m_userName;
    QString m_host;
};

QMap<QString,TorcHTTPHandler*> gHandlers;
QReadWriteLock*                gHandlersLock = new QReadWriteLock(QReadWriteLock::Recursive);
QString                        gServicesDirectory(SERVICES_DIRECTORY);

QMap<QString,WebSocketAuthentication> gWebSocketTokens;
QMutex* gWebSocketTokensLock = new QMutex(QMutex::Recursive);

void TorcHTTPServer::RegisterHandler(TorcHTTPHandler *Handler)
{
    if (!Handler)
        return;

    bool changed = false;

    {
        QWriteLocker locker(gHandlersLock);

        QString signature = Handler->Signature();
        if (gHandlers.contains(signature))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Handler '%1' for '%2' already registered - ignoring").arg(Handler->Name()).arg(signature));
        }
        else if (!signature.isEmpty())
        {
            LOG(VB_GENERAL, LOG_DEBUG, QString("Added handler '%1' for %2").arg(Handler->Name()).arg(signature));
            gHandlers.insert(signature, Handler);
            changed = true;
        }
    }

    if (changed && gWebServer)
        emit gWebServer->HandlersChanged();
}

void TorcHTTPServer::DeregisterHandler(TorcHTTPHandler *Handler)
{
    if (!Handler)
        return;

    bool changed = false;

    {
        QWriteLocker locker(gHandlersLock);

        QMap<QString,TorcHTTPHandler*>::iterator it = gHandlers.find(Handler->Signature());
        if (it != gHandlers.end())
        {
            gHandlers.erase(it);
            changed = true;
        }
    }

    if (changed && gWebServer)
        emit gWebServer->HandlersChanged();
}

void TorcHTTPServer::HandleRequest(TorcHTTPConnection *Connection, TorcHTTPRequest *Request)
{
    if (Request && Connection)
    {
        QReadLocker locker(gHandlersLock);

        QString path = Request->GetPath();

        QMap<QString,TorcHTTPHandler*>::const_iterator it = gHandlers.find(path);
        if (it != gHandlers.end())
        {
            // direct path match
            if (Connection->GetServer()->Authenticated(Connection, Request))
                (*it)->ProcessHTTPRequest(Request, Connection);
        }
        else
        {
            // fully recursive handler
            it = gHandlers.begin();
            for ( ; it != gHandlers.end(); ++it)
            {
                if ((*it)->GetRecursive() && path.startsWith(it.key()))
                {
                    if (Connection->GetServer()->Authenticated(Connection, Request))
                        (*it)->ProcessHTTPRequest(Request, Connection);
                    break;
                }
            }
        }
    }
}

/*! \brief Process an incoming RPC request.
 *
 * Remote procedure calls are only allowed for 'service' directories and will be ignored
 * for anything else (e.g. attempts to retrieve static content and other files).
 *
 * \note The structure of responses is largely consistent with the JSON-RPC 2.0 specification. As such,
 * successful requests will have a 'result' entry and failed requests will have an 'error' field with
 * an error code and error message.
*/
QVariantMap TorcHTTPServer::HandleRequest(const QString &Method, const QVariant &Parameters, QObject *Connection)
{
    QReadLocker locker(gHandlersLock);

    QString path = "/";
    int index = Method.lastIndexOf("/");
    if (index > -1)
        path = Method.left(index + 1).trimmed();

    // ensure this is a services call
    if (path.startsWith(gServicesDirectory))
    {
        // NB no recursive path handling here
        QMap<QString,TorcHTTPHandler*>::const_iterator it = gHandlers.find(path);
        if (it != gHandlers.end())
            return (*it)->ProcessRequest(Method, Parameters, Connection);
    }

    LOG(VB_GENERAL, LOG_ERR, QString("Method '%1' not found in services").arg(Method));

    QVariantMap result;
    QVariantMap error;
    error.insert("code", -32601);
    error.insert("message", "Method not found");
    result.insert("error", error);
    return result;
}

QVariantMap TorcHTTPServer::GetServiceHandlers(void)
{
    QReadLocker locker(gHandlersLock);

    QVariantMap result;

    QMap<QString,TorcHTTPHandler*>::const_iterator it = gHandlers.begin();
    for ( ; it != gHandlers.end(); ++it)
    {
        TorcHTTPService *service = dynamic_cast<TorcHTTPService*>(it.value());
        if (service)
        {
            QVariantMap map;
            map.insert("path", it.key());
            map.insert("name", service->GetUIName());
            result.insert(it.value()->Name(), map);
        }
    }
    return result;
}

void TorcHTTPServer::UpgradeSocket(TorcHTTPRequest *Request, QTcpSocket *Socket)
{
    QMutexLocker locker(gWebServerLock);

    if (gWebServer && Request && Socket)
    {
        // at this stage, the remote device has sent a valid upgrade request, Request contains
        // the appropriate response and Socket still resides in a QRunnable. The remote device
        // should not send any more data until it has received a response.

        // firstly, move the Socket into the server thread, which must be done from the QRunnable
        Socket->moveToThread(gWebServer->thread());

        // and process the upgrade from the server thread
        QMetaObject::invokeMethod(gWebServer, "HandleUpgrade", Qt::AutoConnection, Q_ARG(TorcHTTPRequest*, Request), Q_ARG(QTcpSocket*, Socket));
    }
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

/*! \class TorcHTTPServer
 *  \brief An HTTP server.
 *
 * TorcHTTPServer listens for incoming TCP connections. The default port is 4840
 * though any available port may be used if the default is unavailable. New
 * connections are passed to instances of TorcHTTPConnection.
 *
 * Register new content handlers with RegisterHandler and remove
 * them with DeregisterHandler. These can then be used with the static
 * HandleRequest methods.
 *
 * \sa TorcHTTPRequest
 * \sa TorcHTTPHandler
 * \sa TorcHTTPConnection
 *
 * \todo Fix potential socket and request leak in UpgradeSocket.
 * \todo Add setting for authentication (and hence full username/password requirement).
 * \todo Add setting for SSL.
*/

TorcHTTPServer* TorcHTTPServer::gWebServer = NULL;
QMutex*         TorcHTTPServer::gWebServerLock = new QMutex(QMutex::Recursive);
QString         TorcHTTPServer::gPlatform = QString("");

TorcHTTPServer::TorcHTTPServer()
  : QTcpServer(),
    m_enabled(NULL),
    m_port(NULL),
    m_requiresAuthentication(true),
    m_defaultHandler(NULL),
    m_servicesHelpHandler(NULL),
    m_staticContent(NULL),
    m_abort(0),
    m_httpBonjourReference(0),
    m_torcBonjourReference(0),
    m_webSocketsLock(new QMutex(QMutex::Recursive))
{
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
        connect(parent,    SIGNAL(ValueChanged(bool)),  m_enabled, SLOT(SetActive(bool)));
        connect(parent,    SIGNAL(ActiveChanged(bool)), m_enabled, SLOT(SetActive(bool)));
        connect(m_enabled, SIGNAL(ValueChanged(bool)),  this,      SLOT(Enable(bool)));
        connect(m_enabled, SIGNAL(ActiveChanged(bool)), this,      SLOT(Enable(bool)));
    }

    // port setting - this could become a user editable setting
    m_port = new TorcSetting(NULL, TORC_CORE + "WebServerPort", QString(), TorcSetting::Integer, true, QVariant((int)4840));

    // initialise platform name
    static bool initialised = false;
    if (!initialised)
    {
        initialised = true;
        gPlatform = QString("%1, Version: %2 ").arg(QCoreApplication::applicationName()).arg(TORC_SOURCE_VERSION);
#ifdef Q_OS_WIN
        gPlatform += QString("(Windows %1.%2)").arg(LOBYTE(LOWORD(GetVersion()))).arg(HIBYTE(LOWORD(GetVersion())));
#else
        struct utsname info;
        uname(&info);
        gPlatform += QString("(%1 %2)").arg(info.sysname).arg(info.release);
#endif
    }

    // add the default top level handler
    m_defaultHandler = new TorcHTMLHandler("", QCoreApplication::applicationName());
    RegisterHandler(m_defaultHandler);

    // services help
    m_servicesHelpHandler = new TorcHTMLServicesHelp(this);

    // static files
    m_staticContent = new TorcHTMLStaticContent();
    RegisterHandler(m_staticContent);

    // set thread pool max size
    m_connectionPool.setMaxThreadCount(50);

    // and start
    // NB this will start and stop purely on the basis of the setting, irrespective
    // of network availability and hence is still available via 'localhost'
    Enable(true);
}

TorcHTTPServer::~TorcHTTPServer()
{
    disconnect();

    delete m_defaultHandler;
    delete m_servicesHelpHandler;
    delete m_staticContent;

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

    delete m_webSocketsLock;
}

/*! \brief Retrieve an authentication token for the given request.
 *
 * The request must contain a valid 'Authorization' header for a known user, in which case
 * a unique token is returned. The token is single use, expires after 10 seconds and must
 * be appended to the url used to open a WebSocket with the server (e.g. ws://localhost:port?accesstoken=YOURTOKENHERE).
*/
QString TorcHTTPServer::GetWebSocketToken(TorcHTTPConnection *Connection, TorcHTTPRequest *Request)
{
    ExpireWebSocketTokens();

    QString user;
    if (m_requiresAuthentication && Request && Request->Headers()->contains("Authorization") &&
        AuthenticateUser(Request->Headers()->value("Authorization"), user))
    {
        QMutexLocker locker(gWebSocketTokensLock);
        QString uuid = QUuid::createUuid().toString().mid(1, 36);
        QString host = Connection->GetSocket() ? Connection->GetSocket()->peerAddress().toString() : QString("ErrOR");
        gWebSocketTokens.insert(uuid, WebSocketAuthentication(TorcCoreUtils::GetMicrosecondCount(), user, host));
        return uuid;
    }

    return QString("");
}

/*! \brief Ensures remote user is authorised to access this server.
 *
 * The 'Authorization' header is used for standard HTTP requests.
 *
 * Authentication credentials supplied via the url are used for WebSocket upgrade requests - and it is assumed the if the
 * initial request is authenticated, the entire socket is then authenticated for that user. This needs a lot of
 * further work and consideration:
 *
 * - for the remote html based interface, the user will be asked to authenticate but the authentication details are
 *   not available in javascript. Hence an additional authentication form will be needed to supply credentials
 *   to the WebSocket url. There are numerous possible alternative approaches - don't require authentication for
 *   static content (only for the socket, which provides all of the content anyway), perform socket authentication after
 *   connecting by way of a token/salt that is retrieved via the HTTP interface (which will supply the necessary
 *   'Authorization' header.
 * - Basic HTTP authorization does not explicitly handle logging out.
 * - for peers connecting to one another via sockets, some form of centralised user tracking is required....
 *
 * \todo Use proper username and password.
*/
bool TorcHTTPServer::Authenticated(TorcHTTPConnection *Connection, TorcHTTPRequest *Request)
{
    if (!m_requiresAuthentication)
        return true;

    if (Request)
    {
        // explicit authorization header
        QString dummy;
        if (Request->Headers()->contains("Authorization") && AuthenticateUser(Request->Headers()->value("Authorization"), dummy))
            return true;

        // authentication token supplied in the url (WebSocket)
        if (Request->Queries().contains("accesstoken"))
        {
            ExpireWebSocketTokens();

            {
                QMutexLocker locker(gWebSocketTokensLock);
                QString token = Request->Queries().value("accesstoken");

                if (gWebSocketTokens.contains(token))
                {
                    QString host = Connection->GetSocket() ? Connection->GetSocket()->peerAddress().toString() : QString("eRROr");
                    if (gWebSocketTokens.value(token).m_host == host)
                    {
                        gWebSocketTokens.remove(token);
                        return true;
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_ERR, "Host mismatch for websocket authentication");
                    }
                }
            }
        }

        Request->SetResponseType(HTTPResponseNone);
        Request->SetStatus(HTTP_Unauthorized);
        Request->SetResponseHeader("WWW-Authenticate", QString("Basic realm=\"%1\"").arg(QCoreApplication::applicationName()));
    }

    return false;
}

void TorcHTTPServer::ExpireWebSocketTokens(void)
{
    QMutexLocker locker(gWebSocketTokensLock);

    quint64 tooold = TorcCoreUtils::GetMicrosecondCount() - 10000000;

    QStringList old;
    QMap<QString,WebSocketAuthentication>::iterator it = gWebSocketTokens.begin();
    for ( ; it != gWebSocketTokens.end(); ++it)
        if (it.value().m_timeStamp < tooold)
            old.append(it.key());

    foreach (QString expire, old)
        gWebSocketTokens.remove(expire);
}

bool TorcHTTPServer::AuthenticateUser(const QString &Header, QString &UserName)
{
    static QString username("admin");
    static QString password("1234");

    QStringList authentication = Header.split(" ", QString::SkipEmptyParts);

    if (authentication.size() == 2 && authentication[0].trimmed().compare("basic", Qt::CaseInsensitive) == 0)
    {
        QStringList userinfo = QString(QByteArray::fromBase64(authentication[1].trimmed().toUtf8())).split(':');

        if (userinfo.size() == 2 && userinfo[0] == username && userinfo[1] == password)
        {
            UserName = username;
            return true;
        }
    }

    return false;
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
    m_abort = 0;
    int port = m_port->GetValue().toInt();
    bool waslistening = isListening();

    if (!waslistening)
    {
        if (!listen(QHostAddress::Any, port))
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

#if defined(CONFIG_LIBDNS_SD) && CONFIG_LIBDNS_SD
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
#endif
    }

#if defined(CONFIG_LIBDNS_SD) && CONFIG_LIBDNS_SD
    // advertise service if not already doing so
    if (!m_httpBonjourReference || !m_torcBonjourReference)
    {
        // add the 'root' apiversion, as would be returned by '/services/GetVersion'
        int index = TorcHTMLServicesHelp::staticMetaObject.indexOfClassInfo("Version");

        QMap<QByteArray,QByteArray> map;
        map.insert("uuid", gLocalContext->GetUuid().toLatin1());
        map.insert("apiversion", (index > -1) ? TorcHTMLServicesHelp::staticMetaObject.classInfo(index).value() : "unknown");
        map.insert("priority",   QByteArray::number(gLocalContext->GetPriority()));
        map.insert("starttime",  QByteArray::number(gLocalContext->GetStartTime()));

        QByteArray name(QCoreApplication::applicationName().toLatin1());
        name.append(" on ");
        name.append(QHostInfo::localHostName());

        if (!m_httpBonjourReference)
            m_httpBonjourReference = TorcBonjour::Instance()->Register(port, "_http._tcp.", name, map);

        if (!m_torcBonjourReference)
            m_torcBonjourReference = TorcBonjour::Instance()->Register(port, "_torc._tcp", name, map);
    }
#endif
    if (!waslistening)
        LOG(VB_GENERAL, LOG_INFO, QString("Web server listening on port %1").arg(port));
    return true;
}

void TorcHTTPServer::Close(void)
{
#if defined(CONFIG_LIBDNS_SD) && CONFIG_LIBDNS_SD
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
#endif

    // close all pool threads
    m_abort = 1;
    if (!m_connectionPool.waitForDone(30000))
        LOG(VB_GENERAL, LOG_WARNING, "HTTP connection threads are still running");

    // close websocket threads
    {
        QMutexLocker locker(m_webSocketsLock);

        while (!m_webSockets.isEmpty())
        {
            LOG(VB_GENERAL, LOG_INFO, "Closing outstanding websocket");
            TorcWebSocketThread* thread = m_webSockets.takeLast();
            thread->Shutdown();
            delete thread;
        }
    }

    // actually close
    close();

    LOG(VB_GENERAL, LOG_INFO, "Webserver closed");
}

bool TorcHTTPServer::event(QEvent *Event)
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

    return QTcpServer::event(Event);
}

void TorcHTTPServer::incomingConnection(qintptr SocketDescriptor)
{
    m_connectionPool.start(new TorcHTTPConnection(this, SocketDescriptor, &m_abort), 0);
}

void TorcHTTPServer::HandleUpgrade(TorcHTTPRequest *Request, QTcpSocket *Socket)
{
    if (!Request || !Socket)
        return;

    QMutexLocker locker(m_webSocketsLock);

    TorcWebSocketThread *thread = new TorcWebSocketThread(Request, Socket);
    Socket->moveToThread(thread);
    connect(thread, SIGNAL(Finished()), this, SLOT(WebSocketClosed()));

    thread->start();

    m_webSockets.append(thread);
}

void TorcHTTPServer::WebSocketClosed(void)
{
    QThread *thread = static_cast<QThread*>(sender());

    if (thread)
    {
        QMutexLocker locker(m_webSocketsLock);

        for (int i = 0; i < m_webSockets.size(); ++i)
        {
            if (m_webSockets[i] == thread)
            {
                TorcWebSocketThread *ws = m_webSockets.takeAt(i);

                ws->quit();
                ws->wait();
                delete ws;

                LOG(VB_NETWORK, LOG_INFO, "WebSocket thread deleted");

                break;
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
        qRegisterMetaType<TorcHTTPRequest*>();
        qRegisterMetaType<TorcHTTPService*>();
        qRegisterMetaType<QTcpSocket*>();
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


