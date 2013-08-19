#ifndef TORCHTTPCONNECTION_H
#define TORCHTTPCONNECTION_H

// Qt
#include <QBuffer>
#include <QObject>
#include <QRunnable>
#include <QTcpSocket>

// Torc
#include "torccoreexport.h"
#include "torchttpserver.h"

class TorcHTTPRequest;

class TORC_CORE_PUBLIC TorcHTTPReader
{
  public:
    TorcHTTPReader();
   ~TorcHTTPReader();

    void                   Reset    (void);
    bool                   Read     (QTcpSocket *Socket, int *Abort);

    bool                   m_ready;
    bool                   m_requestStarted;
    bool                   m_headersComplete;
    int                    m_headersRead;
    quint64                m_contentLength;
    quint64                m_contentReceived;
    QString                m_method;
    QByteArray            *m_content;
    QMap<QString,QString> *m_headers;

};

class TORC_CORE_PUBLIC TorcHTTPConnection : public QRunnable
{
  public:
    TorcHTTPConnection(TorcHTTPServer *Parent, qintptr SocketDescriptor, int *Abort);
    virtual ~TorcHTTPConnection();

  public:
    void                     run            (void);
    QTcpSocket*              GetSocket      (void);

  protected:
    int                     *m_abort;
    TorcHTTPServer          *m_server;
    qintptr                  m_socketDescriptor;
    QTcpSocket              *m_socket;
};

#endif // TORCHTTPCONNECTION_H
