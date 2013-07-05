#ifndef TORCNETWORKEDCONTEXT_H
#define TORCNETWORKEDCONTEXT_H

// Qt
#include <QObject>
#include <QAbstractListModel>

// Torc
#include "torccoreexport.h"
#include "torclocalcontext.h"

class TorcNetworkService : public QObject
{
    Q_OBJECT

  public:
    TorcNetworkService(const QString &Name, const QString &UUID, int Port, const QStringList &Addresses);

    Q_PROPERTY (QString     m_name      READ GetName      CONSTANT)
    Q_PROPERTY (QString     m_uuid      READ GetUuid      CONSTANT)
    Q_PROPERTY (int         m_port      READ GetPort      CONSTANT)
    Q_PROPERTY (QString     m_uiAddress READ GetAddress   CONSTANT)

  public slots:
    QString         GetName      (void);
    QString         GetUuid      (void);
    int             GetPort      (void);
    QStringList     GetAddresses (void);
    QString         GetAddress   (void);

  private:
    QString         m_name;
    QString         m_uuid;
    int             m_port;
    QString         m_uiAddress;
    QStringList     m_addresses;
};

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
