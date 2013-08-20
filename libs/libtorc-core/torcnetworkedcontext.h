#ifndef TORCNETWORKEDCONTEXT_H
#define TORCNETWORKEDCONTEXT_H

// Qt
#include <QObject>
#include <QAbstractListModel>

// Torc
#include "torccoreexport.h"
#include "torclocalcontext.h"

class TorcWebSocketThread;

class TorcNetworkService : public QObject
{
    Q_OBJECT

  public:
    TorcNetworkService(const QString &Name, const QString &UUID, int Port, const QStringList &Addresses);
    ~TorcNetworkService();

    Q_PROPERTY (QString     m_name         READ GetName         CONSTANT)
    Q_PROPERTY (QString     m_uuid         READ GetUuid         CONSTANT)
    Q_PROPERTY (int         m_port         READ GetPort         CONSTANT)
    Q_PROPERTY (QString     m_uiAddress    READ GetAddress      CONSTANT)
    Q_PROPERTY (qint64      m_startTime    READ GetStartTime    CONSTANT)
    Q_PROPERTY (int         m_priority     READ GetPriority     CONSTANT)
    Q_PROPERTY (QString     m_apiVersion   READ GetAPIVersion   CONSTANT)

  public slots:
    QString         GetName         (void);
    QString         GetUuid         (void);
    int             GetPort         (void);
    QStringList     GetAddresses    (void);
    QString         GetAddress      (void);
    qint64          GetStartTime    (void);
    int             GetPriority     (void);
    QString         GetAPIVersion   (void);

    void            Connected       (void);
    void            Disconnected    (void);

  public:
    void            Connect         (void);
    void            SetStartTime    (qint64 StartTime);
    void            SetPriority     (int    Priority);
    void            SetAPIVersion   (const QString &Version);

  private:
    QString         m_name;
    QString         m_uuid;
    int             m_port;
    QString         m_uiAddress;
    QStringList     m_addresses;
    qint64          m_startTime;
    int             m_priority;
    QString         m_apiVersion;
    int             m_major;
    int             m_minor;
    int             m_revision;

    TorcWebSocketThread  *m_webSocketThread;
};

Q_DECLARE_METATYPE(TorcNetworkService*);

class TORC_CORE_PUBLIC TorcNetworkedContext: public QAbstractListModel, public TorcObservable
{
    friend class TorcNetworkedContextObject;

    Q_OBJECT

  public:
    // QAbstractListModel
    QVariant                   data          (const QModelIndex &Index, int Role) const;
    QHash<int,QByteArray>      roleNames     (void) const;
    int                        rowCount      (const QModelIndex &Parent = QModelIndex()) const;

  protected:
    TorcNetworkedContext();
    ~TorcNetworkedContext();

    bool                       event         (QEvent* Event);

  private:
    QList<TorcNetworkService*> m_discoveredServices;
    QList<QString>             m_serviceList;
    quint32                    m_bonjourBrowserReference;
};

extern TORC_CORE_PUBLIC TorcNetworkedContext *gNetworkedContext;

#endif // TORCNETWORKEDCONTEXT_H
