/* Class TorcPowerWin
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2013
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
#include "torclocaldefs.h"
#include "torccompat.h"
#include "torclogging.h"
#include "torcpowerwin.h"

#include "winnt.h"
#include "PowrProf.h"
#include "reason.h"

/*! \class TorcPowerWin
 *  \brief A power monitoring class for windows
 *
 * TorcPowerWin uses a timer to poll GetSystemPowerStatus for power (battery , AC) changes.
 * In theory we could just rely on the WM_POWERBROADCAST event received via the main window
 * but this doesn't seem to update the battery level.
 * On Vista and above, we could register for updates via RegisterPowerSettingNotification
 * which should achieve the same (but Qt may be doing this already).
 *
 * When we trigger suspend or hibernate ourselves, we do not receive suspend/standby events
 * in the main window, hence we must notify Suspending/Hibernating directly - which MAY cause
 * issues if the suspend request fails.
 *
 * Finally... if this is going to be used by a console application (with no GUI/winow), then
 * we will have to register as a service and use the RegisterServiceCtrlHandlerEx API for
 * updates.
 *
 * \sa UIDirect3D9Window
 * \sa TorcPower
*/

TorcPowerWin::TorcPowerWin(TorcPower *Parent)
  : TorcPowerPriv(Parent),
    m_timer(new QTimer())
{
    bool privileged = false;

    TOKEN_PRIVILEGES privileges;
    HANDLE token;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
    {
        LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &privileges.Privileges[0].Luid);
        privileges.PrivilegeCount = 1;
        privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        AdjustTokenPrivileges(token, false, &privileges, 0, NULL, 0);
        CloseHandle(token);

        privileged = ERROR_SUCCESS == GetLastError();
    }

    if (!privileged)
        LOG(VB_GENERAL, LOG_WARNING, "Failed to acquire suspend/shutdown privileges");

    SYSTEM_POWER_CAPABILITIES capabilities;
    memset(&capabilities, 0, sizeof(SYSTEM_POWER_CAPABILITIES));

    if (privileged && GetPwrCapabilities(&capabilities))
    {
        m_canHibernate = capabilities.SystemS4;
        m_canSuspend   = capabilities.SystemS1 || capabilities.SystemS2 || capabilities.SystemS3;
    }
    else
    {
        LOG(VB_GENERAL, LOG_WARNING, "Failed to retrieve power capabilities");
    }

    m_canRestart   = privileged;
    m_canShutdown  = privileged;

    // start the battery state timer - update once a minute
    UpdateStatus();

    connect(m_timer, SIGNAL(timeout()), this, SLOT(UpdateStatus()));
    m_timer->start(60 * 1000);
}

TorcPowerWin::~TorcPowerWin()
{
    delete m_timer;
}

bool TorcPowerWin::Restart(void)
{
    if (m_canRestart && ExitWindowsEx(EWX_REBOOT  | EWX_FORCE, SHTDN_REASON_MAJOR_OPERATINGSYSTEM | SHTDN_REASON_MINOR_UPGRADE | SHTDN_REASON_FLAG_PLANNED))
        return true;

    LOG(VB_GENERAL, LOG_ERR, "Failed to restart");
    return false;
}

bool TorcPowerWin::Shutdown(void)
{
    if (m_canShutdown && ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, SHTDN_REASON_MAJOR_OPERATINGSYSTEM | SHTDN_REASON_MINOR_UPGRADE | SHTDN_REASON_FLAG_PLANNED))
        return true;

    LOG(VB_GENERAL, LOG_ERR, "Failed to shutdown");
    return false;
}

bool TorcPowerWin::Suspend(void)
{
    if (m_canSuspend && SetSuspendState(false, true, false))
    {
        ((TorcPower*)parent())->Suspending();
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, "Failed to suspend");
    return false;
}

bool TorcPowerWin::Hibernate(void)
{
    if (m_canHibernate && SetSuspendState(true, true, false))
    {
        ((TorcPower*)parent())->Hibernating();
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, "Failed to hibernate");
    return false;
}

void TorcPowerWin::UpdateStatus(void)
{
    SYSTEM_POWER_STATUS status;
    memset(&status, 0, sizeof(SYSTEM_POWER_STATUS));

    if (GetSystemPowerStatus(&status))
    {
        if (status.ACLineStatus)
        {
            m_batteryLevel = TORC_AC_POWER;
            LOG(VB_GENERAL, LOG_INFO, "On AC power");
        }
        else if (status.BatteryLifePercent != 255)
        {
            bool wasalreadylow = m_batteryLevel >= 0 && m_batteryLevel <= TORC_LOWBATTERY_LEVEL;
            m_batteryLevel = status.BatteryLifePercent;
            LOG(VB_GENERAL, LOG_INFO, QString("Battery level %1%").arg(m_batteryLevel));

            if (!wasalreadylow && (m_batteryLevel >= 0 && m_batteryLevel <= TORC_LOWBATTERY_LEVEL))
                ((TorcPower*)parent())->LowBattery();
        }
        else
        {
            m_batteryLevel = TORC_UNKNOWN_POWER;
        }
    }
    else
    {
        m_batteryLevel = TORC_UNKNOWN_POWER;
    }
}
