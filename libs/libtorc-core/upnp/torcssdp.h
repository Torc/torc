#ifndef TORCSSDP_H
#define TORCSSDP_H

// Qt
#include <QObject>

// Torc
#include "torccoreexport.h"
#include "upnp/torcupnp.h"

class TorcSSDPPriv;

class TORC_CORE_PUBLIC TorcSSDP : public QObject
{
    friend class TorcSSDPThread;

    Q_OBJECT

  public:
    static bool      Search             (const QString &Type, QObject *Owner);
    static bool      CancelSearch       (const QString &Type, QObject *Owner);
    static bool      Announce           (const TorcUPNPDescription &Description);
    static bool      CancelAnnounce     (const TorcUPNPDescription &Description);

  protected:
    TorcSSDP();
    ~TorcSSDP();

    static TorcSSDP* Create             (bool Destroy = false);

  protected slots:
    void             SearchPriv         (const QString &Type, QObject *Owner);
    void             CancelSearchPriv   (const QString &Type, QObject *Owner);
    void             AnnouncePriv       (const TorcUPNPDescription Description);
    void             CancelAnnouncePriv (const TorcUPNPDescription Description);
    bool             event              (QEvent *Event);
    void             Read               (void);

  private:
    TorcSSDPPriv    *m_priv;
    int              m_searchTimer;
    int              m_refreshTimer;
};

#endif // TORCSSDP_H
