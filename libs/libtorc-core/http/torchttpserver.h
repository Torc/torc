#ifndef TORCHTTPSERVER_H
#define TORCHTTPSERVER_H

// Qt
#include <QThreadPool>
#include <QTcpServer>
#include <QMutex>

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
typedef int qintptr;
#endif

// Torc
#include "torccoreexport.h"
#include "torcsetting.h"
#include "torchtmlhandler.h"
#include "torchtmlserviceshelp.h"

class TorcHTTPConnection;
class TorcHTTPHandler;

#define SETTING_WEBSERVERENABLED QString(TORC_CORE + "WebServerEnabled")

class TORC_CORE_PUBLIC TorcHTTPServer : public QTcpServer
{
    Q_OBJECT

    friend class TorcHTTPServerObject;

  public:
    static void    RegisterHandler    (TorcHTTPHandler *Handler);
    static void    DeregisterHandler  (TorcHTTPHandler *Handler);
    static int     GetPort            (void);
    static bool    IsListening        (void);
    static QString PlatformName       (void);

  public:
    virtual       ~TorcHTTPServer     ();
    QMap<QString,QString> GetServiceHandlers (void);
    void           HandleRequest      (TorcHTTPConnection *Connection, TorcHTTPRequest *Request);

  signals:
    void           HandlersChanged    (void);

  protected slots:
    void           UpdateHandlers     (void);

  protected:
    TorcHTTPServer ();
    void           incomingConnection (qintptr SocketDescriptor);
    bool           event              (QEvent *Event);
    bool           Open               (void);
    void           Close              (void);
    void           AddHandler         (TorcHTTPHandler *Handler);
    void           RemoveHandler      (TorcHTTPHandler *Handler);

  protected:
    static TorcHTTPServer*            gWebServer;
    static QMutex*                    gWebServerLock;
    static QString                    gPlatform;

  private slots:
    void           Enable             (bool Enable);

  private:
    TorcSetting                      *m_enabled;
    TorcSetting                      *m_port;
    TorcHTMLHandler                  *m_defaultHandler;
    TorcHTMLServicesHelp             *m_servicesHelpHandler;
    QString                           m_servicesDirectory;

    QThreadPool                       m_connectionPool;
    int                               m_abort;

    QMap<QString,TorcHTTPHandler*>    m_handlers;
    QMutex*                           m_handlersLock;
    QList<TorcHTTPHandler*>           m_newHandlers;
    QMutex*                           m_newHandlersLock;
    QList<TorcHTTPHandler*>           m_oldHandlers;
    QMutex*                           m_oldHandlersLock;

    quint32                           m_httpBonjourReference;
    quint32                           m_torcBonjourReference;
};

#endif // TORCHTTPSERVER_H
