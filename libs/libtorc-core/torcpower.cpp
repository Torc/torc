/* Class TorcPower/TorcPowerImpl
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012
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
#ifdef Q_OS_MAC
#include "torcpowerosx.h"
#elif CONFIG_QTDBUS
#include "torcpowerunixdbus.h"
#endif

TorcPower *gPower = NULL;
QMutex* TorcPower::gPowerLock = new QMutex(QMutex::Recursive);

/*! \class TorcPowerPriv
 *  \brief The base class for platform specific power implementations.
 *
 *  \sa TorcPower
*/

TorcPowerPriv::TorcPowerPriv(TorcPower *Parent)
  : QObject((QObject*)Parent),
    m_canShutdown(false),
    m_canSuspend(false),
    m_canHibernate(false),
    m_canRestart(false),
    m_batteryLevel(TORC_UNKNOWN_POWER)
{
}

bool TorcPowerPriv::CanShutdown(void)
{
    return m_canShutdown;
}

bool TorcPowerPriv::CanSuspend(void)
{
    return m_canSuspend;
}

bool TorcPowerPriv::CanHibernate(void)
{
    return m_canHibernate;
}

bool TorcPowerPriv::CanRestart(void)
{
    return m_canRestart;
}

int TorcPowerPriv::GetBatteryLevel(void)
{
    return m_batteryLevel;
}

void TorcPowerPriv::Debug(void)
{
    QString caps;

    if (m_canShutdown)
        caps += "Shutdown ";
    if (m_canSuspend)
        caps += "Suspend ";
    if (m_canHibernate)
        caps += "Hibernate ";
    if (m_canRestart)
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
    bool CanShutdown     (void) { return false; }
    bool CanSuspend      (void) { return false; }
    bool CanHibernate    (void) { return false; }
    bool CanRestart      (void) { return false; }
};

/*! \class TorcPower
 *  \brief A generic power status class.
 *
 * TorcPower uses underlying platform implementations to monitor the system's power status
 * and emits appropriate notifications (via TorcLocalContext) when the status changes.
 *
 * The current power status (battery charge level or on mains power) can be queried directly via
 * GetBatteryLevel and the systems ability to Suspend/Shutdown etc can be queried via CanSuspend, CanShutdown,
 * CanHibernate and CanRestart.
 *
 * \sa TorcPowerPriv
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
    m_allowShutdown(false),
    m_allowSuspend(false),
    m_allowHibernate(false),
    m_allowRestart(false),
    m_priv(NULL)
{
#ifdef Q_OS_MAC
    m_priv = new TorcPowerOSX(this);
#elif CONFIG_QTDBUS
    if (TorcPowerUnixDBus::Available())
        m_priv = new TorcPowerUnixDBus(this);
#endif

    if (!m_priv)
        m_priv = new TorcPowerNull();

    m_allowShutdown  = gLocalContext->GetSetting(TORC_CORE + "AllowShutdown", true);
    m_allowSuspend   = gLocalContext->GetSetting(TORC_CORE + "AllowSuspend", true);
    m_allowHibernate = gLocalContext->GetSetting(TORC_CORE + "AllowHibernate", true);
    m_allowRestart   = gLocalContext->GetSetting(TORC_CORE + "AllowRestart", true);

    if (m_priv)
        m_priv->Debug();
}

TorcPower::~TorcPower()
{
    if (m_priv)
        m_priv->deleteLater();
    m_priv = NULL;
}

bool TorcPower::Shutdown(void)
{
    return m_allowShutdown ? m_priv->Shutdown() : false;
}

bool TorcPower::Suspend(void)
{
    return m_allowSuspend ? m_priv->Suspend() : false;
}

bool TorcPower::Hibernate(void)
{
    return m_allowHibernate ? m_priv->Hibernate() : false;
}

bool TorcPower::Restart(void)
{
    return m_allowRestart ? m_priv->Restart() : false;
}

bool TorcPower::CanShutdown(void)
{
    return m_allowShutdown && m_priv->CanShutdown();
}

bool TorcPower::CanSuspend(void)
{
    return m_allowSuspend && m_priv->CanSuspend();
}

bool TorcPower::CanHibernate(void)
{
    return m_allowHibernate && m_priv->CanHibernate();
}

bool TorcPower::CanRestart(void)
{
    return m_allowRestart && m_priv->CanRestart();
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
        if (gLocalContext->GetFlag(Torc::Power))
            TorcPower::Create();
    }

    void Destroy(void)
    {
        TorcPower::TearDown();
    }
} TorcPowerObject;
