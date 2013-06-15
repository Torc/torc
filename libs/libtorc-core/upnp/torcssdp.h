#ifndef TORCSSDP_H
#define TORCSSDP_H

// Qt
#include <QObject>

// Torc
#include "torccoreexport.h"

class TorcSSDPPriv;

class TORC_CORE_PUBLIC TorcSSDP : public QObject
{
    friend class TorcSSDPThread;

    Q_OBJECT

  public:
    enum SSDPAnnounce
    {
        Alive,
        ByeBye
    };

  public:
    static bool      Search        (const QString &Name);
    static bool      Announce      (const QString &Name, SSDPAnnounce Type);

  protected:
    TorcSSDP();
    ~TorcSSDP();

    static TorcSSDP* Create        (bool Destroy = false);

  protected slots:
    void             SearchPriv    (const QString &Name);
    void             AnnouncePriv  (const QString &Name, SSDPAnnounce Type);
    bool             event         (QEvent *Event);
    void             Read          (void);

  private:
    TorcSSDPPriv    *m_priv;
};

#endif // TORCSSDP_H
