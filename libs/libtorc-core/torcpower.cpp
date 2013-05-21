/* Class TorcPower/TorcPowerImpl
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012-13
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Qt
#include <QtGlobal>

// Torc
#include "torclocalcontext.h"
#include "torcadminthread.h"
#include "torcpower.h"

TorcPower *gPower = NULL;
QMutex* TorcPower::gPowerLock = new QMutex(QMutex::Recursive);

/*! \class TorcPowerPriv
 *  \brief The base class for platform specific power implementations.
 *
 *  \sa TorcPower
*/

TorcPowerPriv* TorcPowerPriv::Create(TorcPower *Parent)
{
    TorcPowerPriv *power = NULL;

    int score = 0;
    PowerFactory* factory = PowerFactory::GetPowerFactory();
    for ( ; factory; factory = factory->NextFactory())
        (void)factory->Score(score);

    factory = PowerFactory::GetPowerFactory();
    for ( ; factory; factory = factory->NextFactory())
    {
        power = factory->Create(score, Parent);
        if (power)
            break;
    }

    if (!power)
        LOG(VB_GENERAL, LOG_ERR, "Failed to create power implementation");

    return power;
}

TorcPowerPriv::TorcPowerPriv(TorcPower *Parent)
  : QObject(static_cast<QObject*>(Parent)),
    m_batteryLevel(TORC_UNKNOWN_POWER)
{
    m_canShutdown  = new TorcSetting(NULL, QString("CanShutdown"),  QString(), TorcSetting::Checkbox, false, QVariant((bool)false));
    m_canSuspend   = new TorcSetting(NULL, QString("CanSuspend"),   QString(), TorcSetting::Checkbox, false, QVariant((bool)false));
    m_canHibernate = new TorcSetting(NULL, QString("CanHibernate"), QString(), TorcSetting::Checkbox, false, QVariant((bool)false));
    m_canRestart   = new TorcSetting(NULL, QString("CanRestart"),   QString(), TorcSetting::Checkbox, false, QVariant((bool)false));
}

TorcPowerPriv::~TorcPowerPriv()
{
    if (m_canShutdown)
        m_canShutdown->DownRef();

    if (m_canSuspend)
        m_canSuspend->DownRef();

    if (m_canHibernate)
        m_canHibernate->DownRef();

    if (m_canRestart)
        m_canRestart->DownRef();

    m_canShutdown  = NULL;
    m_canSuspend   = NULL;
    m_canHibernate = NULL;
    m_canRestart   = NULL;
}

int TorcPowerPriv::GetBatteryLevel(void)
{
    return m_batteryLevel;
}

void TorcPowerPriv::Debug(void)
{
    QString caps;

    if (m_canShutdown->GetValue().toBool())
        caps += "Shutdown ";
    if (m_canSuspend->GetValue().toBool())
        caps += "Suspend ";
    if (m_canHibernate->GetValue().toBool())
        caps += "Hibernate ";
    if (m_canRestart->GetValue().toBool())
        caps += "Restart ";

    if (caps.isEmpty())
        caps = "None";

    LOG(VB_GENERAL, LOG_INFO, QString("Power support: %1").arg(caps));
}


/*! \class TorcPowerNull
 *  \brief A dummy power implementation.
*/

class TorcPowerNull : public TorcPowerPriv
{
  public:
    TorcPowerNull() : TorcPowerPriv(NULL) { }
   ~TorcPowerNull()      { }
    bool Shutdown        (void) { return false; }
    bool Suspend         (void) { return false; }
    bool Hibernate       (void) { return false; }
    bool Restart         (void) { return false; }
    void Refresh         (void) {               }
};

class PowerFactoryNull : public PowerFactory
{
    void Score(int &Score)
    {
        if (Score <= 1)
            Score = 1;
    }

    TorcPowerPriv* Create(int Score, TorcPower *Parent)
    {
        (void)Parent;

        if (Score <= 1)
            return new TorcPowerNull();

        return NULL;
    }
} PowerFactoryNull;

/*! \class PowerFactory
 *
 *  \sa TorcPower
*/

PowerFactory* PowerFactory::gPowerFactory = NULL;

PowerFactory::PowerFactory()
{
    nextPowerFactory = gPowerFactory;
    gPowerFactory = this;
}

PowerFactory::~PowerFactory()
{
}

PowerFactory* PowerFactory::GetPowerFactory(void)
{
    return gPowerFactory;
}

PowerFactory* PowerFactory::NextFactory(void) const
{
    return nextPowerFactory;
}

/*! \class TorcPower
 *  \brief A generic power status class.
 *
 * TorcPower uses underlying platform implementations to monitor the system's power status
 * and emits appropriate notifications (via TorcLocalContext) when the status changes.
 * Additional implementations can be added by sub-classing PowerFactory and TorcPowerPriv.
 *
 * The current power status (battery charge level or on mains power) can be queried directly via
 * GetBatteryLevel and the system's ability to Suspend/Shutdown etc can be queried via CanSuspend, CanShutdown,
 * CanHibernate and CanRestart.
 *
 * A singleton power object is created by TorcPowerObject from within the administration thread
 * - though TorcPower may be accessed from multiple threads and hence implementations must
 * guard against concurrent access if necessary.
 *
 * \sa PowerFactory
 * \sa TorcPowerPriv
 * \sa TorcPowerObject
*/

void TorcPower::Create(void)
{
    QMutexLocker lock(gPowerLock);

    if (gPower)
        return;

    gPower = new TorcPower();
}

void TorcPower::TearDown(void)
{
    QMutexLocker lock(gPowerLock);

    if (gPower)
        gPower->deleteLater();
    gPower = NULL;
}

TorcPower::TorcPower()
  : QObject(),
    TorcHTTPService(this, "/power", tr("Power"), TorcPower::staticMetaObject, "ShuttingDown,Suspending,Hibernating,Restarting,WokeUp,LowBattery"),
    m_lastBatteryLevel(TORC_UNKNOWN_POWER),
    m_priv(TorcPowerPriv::Create(this))
{
    m_powerGroupItem = new TorcSettingGroup(gRootSetting, tr("Power"));
    m_powerEnabled   = new TorcSetting(m_powerGroupItem, QString(TORC_CORE + "EnablePower"),
                                       tr("Enable power management"),
                                       TorcSetting::Checkbox, true, QVariant((bool)true));
    m_allowShutdown  = new TorcSetting(m_powerEnabled, QString(TORC_CORE + "AllowShutdown"),
                                       tr("Allow Torc to shutdown the system."),
                                       TorcSetting::Checkbox, true, QVariant((bool)true));
    m_allowSuspend   = new TorcSetting(m_powerEnabled, QString(TORC_CORE + "AllowSuspend"),
                                       tr("Allow Torc to suspend the system."),
                                       TorcSetting::Checkbox, true, QVariant((bool)true));
    m_allowHibernate = new TorcSetting(m_powerEnabled, QString(TORC_CORE + "AllowHibernate"),
                                       tr("Allow Torc to hibernate the system."),
                                       TorcSetting::Checkbox, true, QVariant((bool)true));
    m_allowRestart   = new TorcSetting(m_powerEnabled, QString(TORC_CORE + "AllowRestart"),
                                       tr("Allow Torc to restart the system."),
                                       TorcSetting::Checkbox, true, QVariant((bool)true));

    m_powerEnabled->SetActive(gLocalContext->FlagIsSet(Torc::Power));
    // 'allow' depends on both underlying platform capabilities and top level setting
    m_allowShutdown->SetActiveThreshold(2);
    m_allowSuspend->SetActiveThreshold(2);
    m_allowHibernate->SetActiveThreshold(2);
    m_allowRestart->SetActiveThreshold(2);

    if (m_priv->m_canShutdown->GetValue().toBool())
        m_allowShutdown->SetActive(true);
    if (m_priv->m_canSuspend->GetValue().toBool())
        m_allowSuspend->SetActive(true);
    if (m_priv->m_canHibernate->GetValue().toBool())
        m_allowHibernate->SetActive(true);
    if (m_priv->m_canRestart->GetValue().toBool())
        m_allowRestart->SetActive(true);

    if (m_powerEnabled->GetValue().toBool())
    {
        m_allowShutdown->SetActive(true);
        m_allowSuspend->SetActive(true);
        m_allowHibernate->SetActive(true);
        m_allowRestart->SetActive(true);
    }

    // listen for changes
    connect(m_priv->m_canShutdown,  SIGNAL(ValueChanged(bool)), m_allowShutdown,  SLOT(SetActive(bool)));
    connect(m_priv->m_canSuspend,   SIGNAL(ValueChanged(bool)), m_allowSuspend,   SLOT(SetActive(bool)));
    connect(m_priv->m_canHibernate, SIGNAL(ValueChanged(bool)), m_allowHibernate, SLOT(SetActive(bool)));
    connect(m_priv->m_canRestart,   SIGNAL(ValueChanged(bool)), m_allowRestart,   SLOT(SetActive(bool)));

    connect(m_powerEnabled,         SIGNAL(ValueChanged(bool)), m_allowShutdown,  SLOT(SetActive(bool)));
    connect(m_powerEnabled,         SIGNAL(ValueChanged(bool)), m_allowSuspend,   SLOT(SetActive(bool)));
    connect(m_powerEnabled,         SIGNAL(ValueChanged(bool)), m_allowHibernate, SLOT(SetActive(bool)));
    connect(m_powerEnabled,         SIGNAL(ValueChanged(bool)), m_allowRestart,   SLOT(SetActive(bool)));

    if (m_priv)
        m_priv->Debug();
}

TorcPower::~TorcPower()
{
    if (m_allowShutdown)
    {
        m_allowShutdown->Remove();
        m_allowShutdown->DownRef();
    }

    if (m_allowRestart)
    {
        m_allowRestart->Remove();
        m_allowRestart->DownRef();
    }

    if (m_allowHibernate)
    {
        m_allowHibernate->Remove();
        m_allowHibernate->DownRef();
    }

    if (m_allowSuspend)
    {
        m_allowSuspend->Remove();
        m_allowSuspend->DownRef();
    }

    if (m_powerEnabled)
    {
        m_powerEnabled->Remove();
        m_powerEnabled->DownRef();
    }

    if (m_powerGroupItem)
    {
        m_powerGroupItem->Remove();
        m_powerEnabled->DownRef();
    }

    if (m_priv)
        m_priv->deleteLater();

    m_allowShutdown  = NULL;
    m_allowRestart   = NULL;
    m_allowHibernate = NULL;
    m_allowSuspend   = NULL;
    m_powerEnabled   = NULL;
    m_powerGroupItem = NULL;
    m_priv           = NULL;
}

void TorcPower::BatteryUpdated(int Level)
{
    if (m_lastBatteryLevel == Level)
        return;

    bool wasalreadylow = m_lastBatteryLevel >= 0 && m_lastBatteryLevel <= TORC_LOWBATTERY_LEVEL;
    m_lastBatteryLevel = Level;

    if (m_lastBatteryLevel == TORC_AC_POWER)
        LOG(VB_GENERAL, LOG_INFO, "On AC power");
    else if (m_lastBatteryLevel == TORC_UNKNOWN_POWER)
        LOG(VB_GENERAL, LOG_INFO, "Unknown power status");
    else
        LOG(VB_GENERAL, LOG_INFO, QString("Battery level %1%").arg(m_lastBatteryLevel));

    if (!wasalreadylow && (m_lastBatteryLevel >= 0 && m_lastBatteryLevel <= TORC_LOWBATTERY_LEVEL))
        LowBattery();
}

bool TorcPower::Shutdown(void)
{
    return (m_allowShutdown->GetValue().toBool() && m_allowShutdown->IsActive()) ? m_priv->Shutdown() : false;
}

bool TorcPower::Suspend(void)
{
    return (m_allowSuspend->GetValue().toBool() && m_allowSuspend->IsActive()) ? m_priv->Suspend() : false;
}

bool TorcPower::Hibernate(void)
{
    return (m_allowHibernate->GetValue().toBool() && m_allowHibernate->IsActive()) ? m_priv->Hibernate() : false;
}

bool TorcPower::Restart(void)
{
    return (m_allowRestart->GetValue().toBool() && m_allowRestart->IsActive()) ? m_priv->Restart() : false;
}

bool TorcPower::CanShutdown(void)
{
    return m_allowShutdown->GetValue().toBool() && m_allowShutdown->IsActive();
}

bool TorcPower::CanSuspend(void)
{
    return m_allowSuspend->GetValue().toBool() && m_allowSuspend->IsActive();
}

bool TorcPower::CanHibernate(void)
{
    return m_allowHibernate->GetValue().toBool() && m_allowHibernate->IsActive();
}

bool TorcPower::CanRestart(void)
{
    return m_allowRestart->GetValue().toBool() && m_allowRestart->IsActive();
}

int TorcPower::GetBatteryLevel(void)
{
    return m_priv->GetBatteryLevel();
}

void TorcPower::ShuttingDown(void)
{
    LOG(VB_GENERAL, LOG_INFO, "System will shut down");
    TorcLocalContext::NotifyEvent(Torc::ShuttingDown);
}

void TorcPower::Suspending(void)
{
    LOG(VB_GENERAL, LOG_INFO, "System will go to sleep");
    TorcLocalContext::NotifyEvent(Torc::Suspending);
}

void TorcPower::Hibernating(void)
{
    LOG(VB_GENERAL, LOG_INFO, "System will hibernate");
    TorcLocalContext::NotifyEvent(Torc::Hibernating);
}

void TorcPower::Restarting(void)
{
    LOG(VB_GENERAL, LOG_INFO, "System restarting");
    TorcLocalContext::NotifyEvent(Torc::Restarting);
}

void TorcPower::WokeUp(void)
{
    LOG(VB_GENERAL, LOG_INFO, "System woke up");
    TorcLocalContext::NotifyEvent(Torc::WokeUp);
}

void TorcPower::LowBattery(void)
{
    LOG(VB_GENERAL, LOG_INFO, "Sending low battery warning");
    TorcLocalContext::NotifyEvent(Torc::LowBattery);
}

void TorcPower::Refresh(void)
{
    if (m_priv)
        m_priv->Refresh();
}

/*! \class TorcPowerObject
 *  \brief A static class used to create the TorcPower singleton in the admin thread.
*/
static class TorcPowerObject : public TorcAdminObject
{
  public:
    TorcPowerObject()
      : TorcAdminObject(TORC_ADMIN_MED_PRIORITY)
    {
    }

    void Create(void)
    {
        if (gLocalContext->FlagIsSet(Torc::Power))
            TorcPower::Create();
    }

    void Destroy(void)
    {
        TorcPower::TearDown();
    }
} TorcPowerObject;
