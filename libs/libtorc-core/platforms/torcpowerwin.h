#ifndef TORCPOWERWIN_H
#define TORCPOWERWIN_H

// Qt
#include <QTimer>

// Torc
#include "torcpower.h"

class TorcPowerWin : public TorcPowerPriv
{
    Q_OBJECT

  public:
    TorcPowerWin(TorcPower *Parent);
    virtual ~TorcPowerWin();

    bool     Shutdown        (void);
    bool     Suspend         (void);
    bool     Hibernate       (void);
    bool     Restart         (void);

  public slots:
    void     UpdateStatus    (void);

  private:
    QTimer  *m_timer;
};

#endif // TORCPOWERWIN_H
