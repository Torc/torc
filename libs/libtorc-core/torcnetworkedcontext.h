#ifndef TORCNETWORKEDCONTEXT_H
#define TORCNETWORKEDCONTEXT_H

// Qt
#include <QObject>

// Torc
#include "torccoreexport.h"
#include "torclocalcontext.h"

class TorcPeer
{
  public:
    TorcPeer(const QString &Uuid, int Port, const QStringList &Addresses);

    QString         m_uuid;
    int             m_port;
    QStringList     m_addresses;
};

class TORC_CORE_PUBLIC TorcNetworkedContext: public QObject, public TorcObservable
{
    friend class TorcNetworkedContextObject;

    Q_OBJECT

  protected:
    TorcNetworkedContext();
    ~TorcNetworkedContext();

    bool            event  (QEvent* Event);

  private:
    QMap<QString,TorcPeer> m_knownPeers;
    quint32                m_bonjourBrowserReference;
};

#endif // TORCNETWORKEDCONTEXT_H
