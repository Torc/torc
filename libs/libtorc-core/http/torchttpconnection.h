#ifndef TORCHTTPCONNECTION_H
#define TORCHTTPCONNECTION_H

// Qt
#include <QBuffer>
#include <QObject>
#include <QRunnable>
#include <QTcpSocket>

// Torc
#include "torccoreexport.h"

class TorcHTTPServer;
class TorcHTTPRequest;

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
