#ifndef TORCHTTPCONNECTION_H
#define TORCHTTPCONNECTION_H

// Qt
#include <QBuffer>
#include <QObject>
#include <QTcpSocket>

// Torc
#include "torccoreexport.h"
#include "torcreferencecounted.h"

class TorcHTTPServer;
class TorcHTTPRequest;

class TORC_CORE_PUBLIC TorcHTTPConnection : public QObject, public TorcReferenceCounter
{
    Q_OBJECT

  public:
    TorcHTTPConnection(TorcHTTPServer *Parent, QTcpSocket *Socket);

  signals:
    void                     NewRequest     (void);

  public slots:
    void                     ReadFromClient (void);

  public:
    bool                     HasRequests    (void);
    TorcHTTPRequest*         GetRequest     (void);
    void                     Complete       (TorcHTTPRequest* Request);
    QTcpSocket*              Socket         (void);

  protected slots:
    void                     ReadInternal   (void);

  protected:
    virtual ~TorcHTTPConnection();
    void                     ProcessHeader  (const QByteArray &Line, bool Started);
    void                     Reset          (void);

  protected:
    TorcHTTPServer          *m_server;
    QTcpSocket              *m_socket;
    bool                     m_requestStarted;
    bool                     m_headersComplete;
    int64_t                  m_contentLength;
    int64_t                  m_contentReceived;
    QString                  m_method;
    QMap<QString,QString>   *m_headers;
    QByteArray              *m_content;
    QBuffer                  m_buffer;
    QList<TorcHTTPRequest*>  m_requests;
    QString                  m_peerAddress;
};

#endif // TORCHTTPCONNECTION_H
