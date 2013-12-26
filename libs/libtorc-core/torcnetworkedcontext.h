#ifndef TORCNETWORKEDCONTEXT_H
#define TORCNETWORKEDCONTEXT_H

// Qt
#include <QObject>
#include <QAbstractListModel>

// Torc
#include "torccoreexport.h"
#include "http/torchttpservice.h"
#include "torclocalcontext.h"

class TorcRPCRequest;
class TorcNetworkRequest;
class TorcWebSocketThread;
class TorcHTTPRequest;
class QTcpSocket;

class TORC_CORE_PUBLIC TorcNetworkService : public QObject
{
    Q_OBJECT

  public:
    TorcNetworkService(const QString &Name, const QString &UUID, int Port, const QStringList &Addresses);
    ~TorcNetworkService();

    Q_PROPERTY (QString     name              READ GetName         CONSTANT)
    Q_PROPERTY (QString     uuid              READ GetUuid         CONSTANT)
    Q_PROPERTY (int         port              READ GetPort         CONSTANT)
    Q_PROPERTY (QString     uiAddress         READ GetAddress      CONSTANT)
    Q_PROPERTY (qint64      startTime         READ GetStartTime    NOTIFY StartTimeChanged)
    Q_PROPERTY (int         priority          READ GetPriority     NOTIFY PriorityChanged)
    Q_PROPERTY (QString     apiVersion        READ GetAPIVersion   NOTIFY ApiVersionChanged)
    Q_PROPERTY (bool        connected         READ GetConnected    NOTIFY ConnectedChanged)

  signals:
    void                    StartTimeChanged  (void);
    void                    PriorityChanged   (void);
    void                    ApiVersionChanged (void);
    void                    ConnectedChanged  (void);

  public slots:
    QString                 GetName           (void);
    QString                 GetUuid           (void);
    int                     GetPort           (void);
    QStringList             GetAddresses      (void);
    QString                 GetAddress        (void);
    qint64                  GetStartTime      (void);
    int                     GetPriority       (void);
    QString                 GetAPIVersion     (void);
    bool                    GetConnected      (void);

    void                    Connect           (void);
    void                    Connected         (void);
    void                    Disconnected      (void);
    void                    RequestReady      (TorcNetworkRequest *Request);
    void                    RequestReady      (TorcRPCRequest     *Request);

  public:
    void                    SetHost           (const QString &Host);
    void                    SetStartTime      (qint64 StartTime);
    void                    SetPriority       (int    Priority);
    void                    SetAPIVersion     (const QString &Version);
    void                    CreateSocket      (TorcHTTPRequest *Request, QTcpSocket *Socket);
    void                    RemoteRequest     (TorcRPCRequest *Request);
    void                    CancelRequest     (TorcRPCRequest *Request);
    QVariant                ToMap             (void);

  private:
    void                    ScheduleRetry     (void);
    void                    QueryPeerDetails  (void);

  private:
    QString                 name;
    QString                 uuid;
    int                     port;
    QString                 uiAddress;
    qint64                  startTime;
    int                     priority;
    QString                 apiVersion;
    bool                    connected;

    QString                 m_debugString;
    QString                 m_host;
    QStringList             m_addresses;
    int                     m_preferredAddress;
    int                     m_abort;
    TorcRPCRequest         *m_getPeerDetailsRPC;
    TorcNetworkRequest     *m_getPeerDetails;
    TorcWebSocketThread    *m_webSocketThread;
    bool                    m_retryScheduled;
    int                     m_retryInterval;
};

Q_DECLARE_METATYPE(TorcNetworkService*);

class TORC_CORE_PUBLIC TorcNetworkedContext: public QAbstractListModel, public TorcHTTPService
{
    friend class TorcNetworkedContextObject;
    friend class TorcNetworkService;

    Q_OBJECT
    Q_CLASSINFO("Version",    "1.0.0")
    Q_CLASSINFO("GetPeers",   "type=peers")
    Q_PROPERTY(QVariantList peers READ GetPeers NOTIFY PeersChanged)

  public:
    // QAbstractListModel
    QVariant                   data                (const QModelIndex &Index, int Role) const;
    QHash<int,QByteArray>      roleNames           (void) const;
    int                        rowCount            (const QModelIndex &Parent = QModelIndex()) const;

    QVariantList               GetPeers            (void);

    // TorcWebSocket
    static void                UpgradeSocket       (TorcHTTPRequest *Request, QTcpSocket *Socket);

    static void                RemoteRequest       (const QString &UUID, TorcRPCRequest *Request);
    static void                CancelRequest       (const QString &UUID, TorcRPCRequest *Request, int Wait = 1000);

  signals:
    void                       PeersChanged        (void);
    void                       PeerConnected       (QString Name, QString UUID);
    void                       PeerDisconnected    (QString Name, QString UUID);
    void                       UpgradeRequest      (TorcHTTPRequest *Request, QTcpSocket *Socket);
    void                       NewRequest          (const QString &UUID, TorcRPCRequest *Request);
    void                       RequestCancelled    (const QString &UUID, TorcRPCRequest *Request);

  public slots:
    void                       SubscriberDeleted   (QObject *Subscriber);

  protected slots:
    void                       HandleUpgrade       (TorcHTTPRequest *Request, QTcpSocket *Socket);
    void                       HandleNewRequest    (const QString &UUID, TorcRPCRequest *Request);
    void                       HandleCancelRequest (const QString &UUID, TorcRPCRequest *Request);

  protected:
    TorcNetworkedContext();
    ~TorcNetworkedContext();

    void                       Connected           (TorcNetworkService* Peer);
    void                       Disconnected        (TorcNetworkService* Peer);
    bool                       event               (QEvent* Event);

  private:
    void                       Add                 (TorcNetworkService* Peer);
    void                       Delete              (const QString &UUID);

  private:
    QList<TorcNetworkService*> m_discoveredServices;
    QReadWriteLock            *m_discoveredServicesLock;
    QList<QString>             m_serviceList;
    quint32                    m_bonjourBrowserReference;
    QVariantMap                peers; // dummy
};

extern TORC_CORE_PUBLIC TorcNetworkedContext *gNetworkedContext;

#endif // TORCNETWORKEDCONTEXT_H
