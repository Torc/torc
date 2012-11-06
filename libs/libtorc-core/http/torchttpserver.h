#ifndef TORCHTTPSERVER_H
#define TORCHTTPSERVER_H

// Qt
#include <QTcpServer>
#include <QMutex>

// Torc
#include "torccoreexport.h"
#include "torchtmlhandler.h"

class TorcHTTPConnection;
class TorcHTTPHandler;

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
    virtual       ~TorcHTTPServer      ();

  signals:
    void           HandlersChanged    (void);

  public slots:
    void           ClientConnected    (void);
    void           ClientDisconnected (void);
    void           UpdateHandlers     (void);
    void           NewRequest         (void);

  protected:
    static void    Create             (void);
    static void    Destroy            (void);

  protected:
    explicit       TorcHTTPServer     ();
    bool           event              (QEvent *Event);
    bool           Open               (void);
    void           Close              (void);
    void           AddHandler         (TorcHTTPHandler *Handler);
    void           RemoveHandler      (TorcHTTPHandler *Handler);
    void           UserServicesHelp   (TorcHTTPRequest *Request, TorcHTTPConnection *Connection);

  protected:
    static TorcHTTPServer*            gWebServer;
    static QMutex*                    gWebServerLock;
    static QString                    gPlatform;

  private:
    TorcHTMLHandler                  *m_defaultHandler;
    QString                           m_servicesDirectory;
    int                               m_port;
    QMap<QTcpSocket*,TorcHTTPConnection*> m_connections;
    QMap<QString,TorcHTTPHandler*>    m_handlers;
    QList<TorcHTTPHandler*>           m_newHandlers;
    QMutex*                           m_newHandlersLock;
    QList<TorcHTTPHandler*>           m_oldHandlers;
    QMutex*                           m_oldHandlersLock;
};

#endif // TORCHTTPSERVER_H
