#ifndef TORCHTTPSERVER_H
#define TORCHTTPSERVER_H

// Qt
#include <QThreadPool>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMutex>

// Torc
#include "torccoreexport.h"
#include "torcsetting.h"
#include "torchtmlhandler.h"
#include "torchtmlserviceshelp.h"
#include "torchtmlstaticcontent.h"
#include "torcwebsocket.h"

class TorcHTTPConnection;
class TorcHTTPHandler;

#define SETTING_WEBSERVERENABLED QString(TORC_CORE + "WebServerEnabled")

class TORC_CORE_PUBLIC TorcHTTPServer : public QTcpServer
{
    Q_OBJECT

    friend class TorcHTTPServerObject;

  public:
    // Content/service handlers
    static void    RegisterHandler    (TorcHTTPHandler *Handler);
    static void    DeregisterHandler  (TorcHTTPHandler *Handler);
    static void    HandleRequest      (TorcHTTPConnection *Connection, TorcHTTPRequest *Request);
    static QVariantMap HandleRequest  (const QString &Method, const QVariant &Parameters, QObject *Connection);
    static QVariantMap GetServiceHandlers (void);

    // WebSockets
    static void    UpgradeSocket      (TorcHTTPRequest *Request, QTcpSocket *Socket);

    // server status
    static int     GetPort            (void);
    static bool    IsListening        (void);
    static QString PlatformName       (void);

  public:
    virtual       ~TorcHTTPServer     ();
    QString        GetWebSocketToken  (TorcHTTPConnection *Connection, TorcHTTPRequest *Request);
    bool           Authenticated      (TorcHTTPConnection *Connection, TorcHTTPRequest *Request);

  signals:
    void           HandlersChanged    (void);

  protected slots:
    // WebSockets
    void           HandleUpgrade      (TorcHTTPRequest *Request, QTcpSocket *Socket);
    void           WebSocketClosed    (void);

  protected:
    TorcHTTPServer ();
    void           incomingConnection (qintptr SocketDescriptor);
    bool           event              (QEvent *Event);
    bool           Open               (void);
    void           Close              (void);

  protected:
    static TorcHTTPServer*            gWebServer;
    static QMutex*                    gWebServerLock;
    static QString                    gPlatform;

  private slots:
    void           Enable             (bool Enable);

  private:
    static void    ExpireWebSocketTokens (void);
    bool           AuthenticateUser   (const QString &Header, QString &UserName);

  private:
    TorcSetting                      *m_enabled;
    TorcSetting                      *m_port;
    bool                              m_requiresAuthentication;
    TorcHTMLHandler                  *m_defaultHandler;
    TorcHTMLServicesHelp             *m_servicesHelpHandler;
    TorcHTMLStaticContent            *m_staticContent;

    QThreadPool                       m_connectionPool;
    int                               m_abort;

    quint32                           m_httpBonjourReference;
    quint32                           m_torcBonjourReference;

    QList<TorcWebSocketThread*>       m_webSockets;
    QMutex*                           m_webSocketsLock;
};

Q_DECLARE_METATYPE(TorcHTTPRequest*);
Q_DECLARE_METATYPE(QTcpSocket*);

#endif // TORCHTTPSERVER_H
