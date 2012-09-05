/* Class TorcPowerOSX
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

// OS X
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>

// Torc
#include "torclocalcontext.h"
#include "torcthread.h"
#include "torccocoa.h"
#include "torcrunlooposx.h"
#include "torcpowerosx.h"

/*! \class TorcPowerOSX
 *  \brief A power monitoring class for OS X.
 *
 * TorcPowerOSX uses the IOKit framework to monitor the system's power status.
 *
 * \sa TorcRunLoopOSX
*/

static OSStatus SendAppleEventToSystemProcess(AEEventID EventToSend);

TorcPowerOSX::TorcPowerOSX(TorcPower *Parent)
  : TorcPowerPriv(Parent),
    m_powerRef(NULL),
    m_rootPowerDomain(NULL),
    m_powerNotifier(MACH_PORT_NULL),
    m_powerNotifyPort(NULL)
{
    CocoaAutoReleasePool pool;

    if (!gAdminRunLoop)
    {
        LOG(VB_GENERAL, LOG_ERR, "OS X callback run loop not present - aborting");
        return;
    }

    // Register for power status updates
    m_rootPowerDomain = IORegisterForSystemPower(this, &m_powerNotifyPort, PowerCallBack, &m_powerNotifier);
    if (m_rootPowerDomain)
    {
        CFRunLoopAddSource(gAdminRunLoop,
                           IONotificationPortGetRunLoopSource(m_powerNotifyPort),
                           kCFRunLoopDefaultMode);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to setup power status callback");
    }

    // Is there a battery?
    CFArrayRef batteryinfo = NULL;

    if (IOPMCopyBatteryInfo(kIOMasterPortDefault, &batteryinfo) == kIOReturnSuccess)
    {
        CFRelease(batteryinfo);

        // register for notification of power source changes
        m_powerRef = IOPSNotificationCreateRunLoopSource(PowerSourceCallBack, this);
        if (m_powerRef)
        {
            CFRunLoopAddSource(gAdminRunLoop, m_powerRef, kCFRunLoopDefaultMode);
            GetPowerStatus();
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to setup power source callback");
        }
    }

    // Set capabilities
    m_canShutdown  = true;
    m_canSuspend   = IOPMSleepEnabled();
    m_canHibernate = true;
    m_canRestart   = true;
}

TorcPowerOSX::~TorcPowerOSX()
{
    CocoaAutoReleasePool pool;

    // deregister power status change notifications
    if (gAdminRunLoop)
    {
        CFRunLoopRemoveSource(gAdminRunLoop,
                              IONotificationPortGetRunLoopSource(m_powerNotifyPort),
                              kCFRunLoopDefaultMode );
        IODeregisterForSystemPower(&m_powerNotifier);
        IOServiceClose(m_rootPowerDomain);
        IONotificationPortDestroy(m_powerNotifyPort);
    }

    // deregister power source change notifcations
    if (m_powerRef && gAdminRunLoop)
    {
        CFRunLoopRemoveSource(gAdminRunLoop, m_powerRef, kCFRunLoopDefaultMode);
        CFRelease(m_powerRef);
    }
}

bool TorcPowerOSX::Shutdown(void)
{
    OSStatus error = SendAppleEventToSystemProcess(kAEShutDown);

    if (noErr == error)
    {
        LOG(VB_GENERAL, LOG_INFO, "Sent shutdown command.");
        TorcLocalContext::NotifyEvent(Torc::ShuttingDown);
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, "Failed to send shutdown command.");
    return false;
}

bool TorcPowerOSX::Suspend(void)
{
    OSStatus error = SendAppleEventToSystemProcess(kAESleep);

    if (noErr == error)
    {
        LOG(VB_GENERAL, LOG_INFO, "Sent sleep command.");
        TorcLocalContext::NotifyEvent(Torc::Suspending);
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, "Failed to send sleep command.");
    return false;
}

bool TorcPowerOSX::Hibernate(void)
{
    return Suspend();
}

bool TorcPowerOSX::Restart(void)
{
    OSStatus error = SendAppleEventToSystemProcess(kAERestart);

    if (noErr == error)
    {
        LOG(VB_GENERAL, LOG_INFO, "Sent restart command.");
        TorcLocalContext::NotifyEvent(Torc::Restarting);
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, "Failed to send restart command.");
    return false;
}

void TorcPowerOSX::GetPowerStatus(void)
{
    if (!m_powerRef)
        return;

    CFTypeRef  info = IOPSCopyPowerSourcesInfo();
    CFArrayRef list = IOPSCopyPowerSourcesList(info);

    for (int i = 0; i < CFArrayGetCount(list); i++)
    {
        CFTypeRef source = CFArrayGetValueAtIndex(list, i);
        CFDictionaryRef description = IOPSGetPowerSourceDescription(info, source);

        if ((CFBooleanRef)CFDictionaryGetValue(description, CFSTR(kIOPSIsPresentKey)) == kCFBooleanFalse)
            continue;

        CFStringRef type = (CFStringRef)CFDictionaryGetValue(description, CFSTR(kIOPSTransportTypeKey));
        if (type && CFStringCompare(type, CFSTR(kIOPSInternalType), 0) == kCFCompareEqualTo)
        {
            CFStringRef state = (CFStringRef)CFDictionaryGetValue(description, CFSTR(kIOPSPowerSourceStateKey));
            if (state && CFStringCompare(state, CFSTR(kIOPSACPowerValue), 0) == kCFCompareEqualTo)
            {
                m_batteryLevel = TORC_AC_POWER;
                LOG(VB_GENERAL, LOG_INFO, "On AC Power");
            }
            else if (state && CFStringCompare(state, CFSTR(kIOPSBatteryPowerValue), 0) == kCFCompareEqualTo)
            {
                bool lowbattery = m_batteryLevel >= 0 && m_batteryLevel <= TORC_LOWBATTERY_LEVEL;
                int32_t current;
                int32_t max;
                CFNumberRef capacity = (CFNumberRef)CFDictionaryGetValue(description, CFSTR(kIOPSCurrentCapacityKey));
                CFNumberGetValue(capacity, kCFNumberSInt32Type, &current);
                capacity = (CFNumberRef)CFDictionaryGetValue(description, CFSTR(kIOPSMaxCapacityKey));
                CFNumberGetValue(capacity, kCFNumberSInt32Type, &max);
                m_batteryLevel = (int)(((qreal)current / ((qreal)max)) * 100.0);
                LOG(VB_GENERAL, LOG_INFO, QString("Battery level %1%").arg(m_batteryLevel));

                if (!lowbattery && (m_batteryLevel >= 0 && m_batteryLevel <= TORC_LOWBATTERY_LEVEL))
                    ((TorcPower*)parent())->LowBattery();
            }
            else
            {
                m_batteryLevel = TORC_UNKNOWN_POWER;
            }
        }
    }

    CFRelease(list);
    CFRelease(info);
}

void TorcPowerOSX::PowerCallBack(void *Reference, io_service_t Service,
                                 natural_t Type, void *Data)
{
    CocoaAutoReleasePool pool;
    TorcPowerOSX* power  = static_cast<TorcPowerOSX*>(Reference);

    if (power && power->parent())
    {
        TorcPower* parent = (TorcPower*)power->parent();
        switch (Type)
        {
            case kIOMessageCanSystemPowerOff:
                IOAllowPowerChange(power->m_rootPowerDomain, (long)Data);
                parent->ShuttingDown();
                break;
            case kIOMessageCanSystemSleep:
                IOAllowPowerChange(power->m_rootPowerDomain, (long)Data);
                parent->Suspending();
                break;
            case kIOMessageSystemWillPowerOff:
                IOAllowPowerChange(power->m_rootPowerDomain, (long)Data);
                parent->ShuttingDown();
                break;
            case kIOMessageSystemWillRestart:
                IOAllowPowerChange(power->m_rootPowerDomain, (long)Data);
                break;
            case kIOMessageSystemWillSleep:
                IOAllowPowerChange(power->m_rootPowerDomain, (long)Data);
                parent->Suspending();
                break;
            case kIOMessageSystemHasPoweredOn:
                parent->WokeUp();
                break;
        }
    }
}

void TorcPowerOSX::PowerSourceCallBack(void *Reference)
{
    CocoaAutoReleasePool pool;
    TorcPowerOSX* power = static_cast<TorcPowerOSX*>(Reference);

    if (power)
        power->GetPowerStatus();
}

// see Technical Q&A QA1134
OSStatus SendAppleEventToSystemProcess(AEEventID EventToSend)
{
    AEAddressDesc targetDesc;
    static const ProcessSerialNumber kPSNOfSystemProcess = { 0, kSystemProcess };
    AppleEvent eventReply       = { typeNull, NULL };
    AppleEvent appleEventToSend = { typeNull, NULL };

    OSStatus error = noErr;

    error = AECreateDesc(typeProcessSerialNumber, &kPSNOfSystemProcess,
                         sizeof(kPSNOfSystemProcess), &targetDesc);

    if (error != noErr)
        return error;

    error = AECreateAppleEvent(kCoreEventClass, EventToSend, &targetDesc,
                               kAutoGenerateReturnID, kAnyTransactionID, &appleEventToSend);

    AEDisposeDesc(&targetDesc);
    if (error != noErr)
        return error;

    error = AESendMessage(&appleEventToSend, &eventReply, kAENormalPriority, kAEDefaultTimeout);

    AEDisposeDesc(&appleEventToSend);

    if (error != noErr)
        return error;

    AEDisposeDesc(&eventReply);

    return error;
}

