#ifndef TORCPOWER_H
#define TORCPOWER_H

// Qt
#include <QObject>
#include <QMutex>

// Torc
#include "torccoreexport.h"
#include "http/torchttpservice.h"

#define TORC_AC_POWER         -1
#define TORC_LOWBATTERY_LEVEL 10
#define TORC_UNKNOWN_POWER    101

class TorcPower;

class TorcPowerPriv : public QObject
{
    Q_OBJECT

  public:
    static TorcPowerPriv* Create(TorcPower *Parent);

    TorcPowerPriv(TorcPower *Parent);
    virtual ~TorcPowerPriv();

    virtual bool Shutdown        (void) = 0;
    virtual bool Suspend         (void) = 0;
    virtual bool Hibernate       (void) = 0;
    virtual bool Restart         (void) = 0;
    virtual void Refresh         (void) = 0;

    bool         CanShutdown     (void);
    bool         CanSuspend      (void);
    bool         CanHibernate    (void);
    bool         CanRestart      (void);
    int          GetBatteryLevel (void);

    void         Debug           (void);

  protected:
    bool         m_canShutdown;
    bool         m_canSuspend;
    bool         m_canHibernate;
    bool         m_canRestart;
    int          m_batteryLevel;
};

class TORC_CORE_PUBLIC PowerFactory
{
  public:
    PowerFactory();
    virtual ~PowerFactory();

    static PowerFactory*   GetPowerFactory   (void);
    PowerFactory*          NextFactory       (void) const;
    virtual void           Score             (int &Score) = 0;
    virtual TorcPowerPriv* Create            (int Score, TorcPower *Parent) = 0;

  protected:
    static PowerFactory* gPowerFactory;
    PowerFactory*        nextPowerFactory;
};

class TORC_CORE_PUBLIC TorcPower : public QObject, public TorcHTTPService
{
    Q_OBJECT

  public:
    static QMutex *gPowerLock;
    static void    Create(void);
    static void    TearDown(void);

  public:
    virtual ~TorcPower();
    void BatteryUpdated  (int Level);

  public slots:
    bool CanShutdown     (void);
    bool CanSuspend      (void);
    bool CanHibernate    (void);
    bool CanRestart      (void);
    int  GetBatteryLevel (void);

    bool Shutdown        (void);
    bool Suspend         (void);
    bool Hibernate       (void);
    bool Restart         (void);

    void ShuttingDown    (void);
    void Suspending      (void);
    void Hibernating     (void);
    void Restarting      (void);
    void WokeUp          (void);
    void LowBattery      (void);
    void Refresh         (void);

  protected:
    TorcPower();

  private:
    bool           m_allowShutdown;
    bool           m_allowSuspend;
    bool           m_allowHibernate;
    bool           m_allowRestart;
    int            m_lastBatteryLevel;
    TorcPowerPriv *m_priv;
};

extern TORC_CORE_PUBLIC TorcPower *gPower;

#endif // TORCPOWER_H
