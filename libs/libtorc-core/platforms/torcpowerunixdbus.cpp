/* Class TorcPowerUnixDBus
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

// Torc
#include "torclogging.h"
#include "torcpowerunixdbus.h"

/*! \class TorcPowerUnixDBus
 *  \brief A power monitoring class for Unix based systems.
 *
 * TorcPowerUnixDBus uses the UPower and ConsoleKit DBus interfaces to monitor the system's power
 * status. UPower has replaced HAL and DeviceKit as the default power monitoring interface but additional
 * implementations for DeviceKit (and perhaps HAL) may be required for complete coverage.
 *
 * \todo Battery status monitoring is untested.
*/

bool TorcPowerUnixDBus::Available(void)
{
    QDBusInterface upower("org.freedesktop.UPower",
                          "/org/freedesktop/UPower",
                          "org.freedesktop.UPower",
                          QDBusConnection::systemBus());
    QDBusInterface consolekit("org.freedesktop.ConsoleKit",
                              "/org/freedesktop/ConsoleKit/Manager",
                              "org.freedesktop.ConsoleKit.Manager",
                              QDBusConnection::systemBus());

    if (upower.isValid() && consolekit.isValid())
    {
        LOG(VB_GENERAL, LOG_INFO, "UPower and ConsoleKit available");
        return true;
    }

    return false;
}

TorcPowerUnixDBus::TorcPowerUnixDBus(TorcPower *Parent)
  : TorcPowerPriv(Parent),
    m_onBattery(false),
    m_deviceLock(new QMutex(QMutex::Recursive))
{
    // create interfaces
    m_upowerInterface = new QDBusInterface("org.freedesktop.UPower",
                                           "/org/freedesktop/UPower",
                                           "org.freedesktop.UPower",
                                           QDBusConnection::systemBus());
    m_consoleInterface = new QDBusInterface("org.freedesktop.ConsoleKit",
                                            "/org/freedesktop/ConsoleKit/Manager",
                                            "org.freedesktop.ConsoleKit.Manager",
                                            QDBusConnection::systemBus());

    if (m_upowerInterface)
    {
        if (!m_upowerInterface->isValid())
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to create UPower interface");
            delete m_upowerInterface;
            m_upowerInterface = NULL;
        }
    }

    if (m_consoleInterface)
    {
        if (!m_consoleInterface->isValid())
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to create ConsoleKit interface");
            delete m_consoleInterface;
            m_consoleInterface = NULL;
        }
    }

    if (m_consoleInterface)
    {
        QDBusReply<bool> shutdown = m_consoleInterface->call(QLatin1String("CanStop"));
        if (shutdown.isValid() && shutdown.value())
            m_canShutdown = true;

        QDBusReply<bool> restart = m_consoleInterface->call(QLatin1String("CanRestart"));
        if (restart.isValid() && restart.value())
            m_canRestart = true;
    }

    // populate devices
    if (m_upowerInterface)
    {
        QDBusReply<QList<QDBusObjectPath> > devicecheck = m_upowerInterface->call(QLatin1String("EnumerateDevices"));

        if (devicecheck.isValid())
        {
            QMutexLocker locker(m_deviceLock);

            foreach (QDBusObjectPath device, devicecheck.value())
                m_devices.insert(device.path(), GetBatteryLevel(device.path()));
        }
    }

    // register for events
    if (!QDBusConnection::systemBus().connect(
         "org.freedesktop.UPower", "/org/freedesktop/UPower",
         "org.freedesktop.UPower", "Sleeping",
         parent(), SLOT(Suspending())))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to register for sleep events");
    }

    if (!QDBusConnection::systemBus().connect(
         "org.freedesktop.UPower", "/org/freedesktop/UPower",
         "org.freedesktop.UPower", "Resuming",
         parent(), SLOT(WokeUp())))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to register for resume events");
    }

    if (!QDBusConnection::systemBus().connect(
         "org.freedesktop.UPower", "/org/freedesktop/UPower",
         "org.freedesktop.UPower", "Changed",
         this, SLOT(Changed())))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to register for Changed");
    }

    if (!QDBusConnection::systemBus().connect(
         "org.freedesktop.UPower", "/org/freedesktop/UPower",
         "org.freedesktop.UPower", "DeviceChanged", "o",
         this, SLOT(DeviceChanged(QDBusObjectPath))))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to register for DeviceChanged");
    }

    if (!QDBusConnection::systemBus().connect(
         "org.freedesktop.UPower", "/org/freedesktop/UPower",
         "org.freedesktop.UPower", "DeviceAdded", "o",
         this, SLOT(DeviceAdded(QDBusObjectPath))))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to register for DeviceAdded");
    }

    if (!QDBusConnection::systemBus().connect(
         "org.freedesktop.UPower", "/org/freedesktop/UPower",
         "org.freedesktop.UPower", "DeviceRemoved", "o",
         this, SLOT(DeviceAdded(QDBusObjectPath))))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to register for DeviceRemoved");
    }

    // set battery state
    Changed();
}

TorcPowerUnixDBus::~TorcPowerUnixDBus()
{
    delete m_upowerInterface;
    delete m_consoleInterface;
    delete m_deviceLock;
}

bool TorcPowerUnixDBus::Shutdown(void)
{
    if (m_consoleInterface && m_canShutdown)
    {
        QList<QVariant> dummy;
        if (m_upowerInterface->callWithCallback(QLatin1String("Stop"), dummy, (QObject*)this, SLOT(DBusCallback()), SLOT(DBusError(QDBusError))))
            return true;

        LOG(VB_GENERAL, LOG_ERR, "Shutdown call failed");
    }

    return false;
}

bool TorcPowerUnixDBus::Suspend(void)
{
    if (m_upowerInterface && m_canSuspend)
    {
        QList<QVariant> dummy;
        if (m_upowerInterface->callWithCallback(QLatin1String("AboutToSleep"), dummy, (QObject*)this, SLOT(DBusCallback()), SLOT(DBusError(QDBusError))))
            if (m_upowerInterface->callWithCallback(QLatin1String("Suspend"), dummy, (QObject*)this, SLOT(DBusCallback()), SLOT(DBusError(QDBusError))))
                return true;

        LOG(VB_GENERAL, LOG_ERR, "Suspend call failed");
    }

    return false;
}

bool TorcPowerUnixDBus::Hibernate(void)
{
    if (m_upowerInterface && m_canHibernate)
    {
        QList<QVariant> dummy;
        if (m_upowerInterface->callWithCallback(QLatin1String("AboutToSleep"), dummy, (QObject*)this, SLOT(DBusCallback()), SLOT(DBusError(QDBusError))))
            if (m_upowerInterface->callWithCallback(QLatin1String("Hibernate"), dummy, (QObject*)this, SLOT(DBusCallback()), SLOT(DBusError(QDBusError))))
                return true;

        LOG(VB_GENERAL, LOG_ERR, "Hibernate call failed");
    }

    return false;
}

bool TorcPowerUnixDBus::Restart(void)
{
    if (m_consoleInterface && m_canRestart)
    {
        QList<QVariant> dummy;
        if (m_upowerInterface->callWithCallback(QLatin1String("Restart"), dummy, (QObject*)this, SLOT(DBusCallback()), SLOT(DBusError(QDBusError))))
            return true;

        LOG(VB_GENERAL, LOG_ERR, "Restart call failed");
    }

    return false;
}

void TorcPowerUnixDBus::DeviceAdded(QDBusObjectPath Device)
{
    {
        QMutexLocker locker(m_deviceLock);

        if (m_devices.contains(Device.path()))
            return;

        m_devices.insert(Device.path(), GetBatteryLevel(Device.path()));
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Added UPower.Device '%1'")
        .arg(Device.path()));

    UpdateBattery();
}

void TorcPowerUnixDBus::DeviceRemoved(QDBusObjectPath Device)
{
    {
        QMutexLocker locker(m_deviceLock);

        if (!m_devices.contains(Device.path()))
            return;

        m_devices.remove(Device.path());
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Removed UPower.Device '%1'")
        .arg(Device.path()));

    UpdateBattery();
}

void TorcPowerUnixDBus::DeviceChanged(QDBusObjectPath Device)
{
    {
        QMutexLocker locker(m_deviceLock);

        if (!m_devices.contains(Device.path()))
            return;

        m_devices[Device.path()] = GetBatteryLevel(Device.path());
    }

    UpdateBattery();
}

void TorcPowerUnixDBus::DBusError(QDBusError Error)
{
    LOG(VB_GENERAL, LOG_ERR, QString("DBus callback error: %1, %2")
        .arg(Error.name()).arg(Error.message().trimmed()));
}

void TorcPowerUnixDBus::DBusCallback(void)
{
}

void TorcPowerUnixDBus::Changed(void)
{
    UpdateProperties();
    UpdateBattery();
}

void TorcPowerUnixDBus::UpdateBattery(void)
{
    bool lowbattery = m_batteryLevel >= 0 && m_batteryLevel <= TORC_LOWBATTERY_LEVEL;

    if (m_onBattery)
    {
        QMutexLocker locker(m_deviceLock);

        qreal total = 0;
        int   count = 0;

        QMap<QString,int>::iterator it = m_devices.begin();
        for ( ; it != m_devices.end(); ++it)
        {
            if (it.value() >= 0 && it.value() <= 100)
            {
                count++;
                total += (qreal)it.value();
            }
        }

        if (count > 0)
        {
            m_batteryLevel = (int)((total / count) + 0.5);
            LOG(VB_GENERAL, LOG_INFO, QString("Battery level %1%").arg(m_batteryLevel));
        }
        else
        {
            m_batteryLevel = TORC_UNKNOWN_POWER;
        }
    }
    else
    {
        m_batteryLevel = TORC_AC_POWER;
    }

    if (!lowbattery && (m_batteryLevel >= 0 && m_batteryLevel <= TORC_LOWBATTERY_LEVEL))
        ((TorcPower*)parent())->LowBattery();
}

int TorcPowerUnixDBus::GetBatteryLevel(const QString &Path)
{
    QDBusInterface interface("org.freedesktop.UPower", Path, "org.freedesktop.UPower.Device",
                             QDBusConnection::systemBus());

    if (interface.isValid())
    {
        QVariant battery = interface.property("IsRechargeable");
        if (battery.isValid() && battery.toBool() == true)
        {
            QVariant percent = interface.property("Percentage");
            if (percent.isValid())
            {
                int result = percent.toFloat() * 100.0;
                if (result >= 0.0 && result <= 100.0)
                    return (int)(result + 0.5);
            }
        }
        else
        {
            return TORC_AC_POWER;
        }
    }

    return TORC_UNKNOWN_POWER;
}

void TorcPowerUnixDBus::UpdateProperties(void)
{
    m_canSuspend   = false;
    m_canHibernate = false;
    m_onBattery    = false;

    if (m_upowerInterface)
    {
        QVariant cansuspend = m_upowerInterface->property("CanSuspend");
        if (cansuspend.isValid() && cansuspend.toBool() == true)
            m_canSuspend = true;

        QVariant canhibernate = m_upowerInterface->property("CanHibernate");
        if (canhibernate.isValid() && canhibernate.toBool() == true)
            m_canHibernate = true;

        QVariant onbattery = m_upowerInterface->property("OnBattery");
        if (onbattery.isValid() && onbattery.toBool() == true)
            m_onBattery = true;
    }
}
