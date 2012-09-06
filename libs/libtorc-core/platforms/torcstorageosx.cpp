/* Class TorcStorageOSX
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
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IODVDMedia.h>
#include <IOKit/storage/IOBDMedia.h>

// Qt
#include <QMutex>

// Torc
#include "torclogging.h"
#include "torcstoragedevice.h"
#include "torcosxutils.h"
#include "torcrunlooposx.h"
#include "torcstorageosx.h"

/*! \class TorcStorageOSX
 *  \brief A class to detect fixed and removable storage media on OS X.
 *
 *  \todo Actually remove USB/flash drives after they've been unmounted.
 *  \todo Move device tracking to TorcStorage.
 */

TorcStorageOSX::TorcStorageOSX(TorcStorage *Parent)
    : TorcStoragePriv(Parent),
      m_daSession(NULL)
{
    // create disk arbitration session
    m_daSession = DASessionCreate(kCFAllocatorDefault);

    // register for updates
    CFDictionaryRef match = kDADiskDescriptionMatchVolumeMountable;
    if (m_daSession)
    {
        DARegisterDiskAppearedCallback(m_daSession, match,
                                       DiskAppearedCallback, this);
        DARegisterDiskDisappearedCallback(m_daSession, match,
                                          DiskDisappearedCallback, this);
        DARegisterDiskDescriptionChangedCallback(m_daSession, match,
                                                 kDADiskDescriptionWatchVolumeName,
                                                 DiskChangedCallback, this);
    }

    if (gAdminRunLoop)
        DASessionScheduleWithRunLoop(m_daSession, gAdminRunLoop, kCFRunLoopDefaultMode);
    else
        LOG(VB_GENERAL, LOG_ERR, "OS X callback run loop not present - aborting");
}

TorcStorageOSX::~TorcStorageOSX()
{
    // unregister for updates
    if (m_daSession)
    {
        DAUnregisterCallback(m_daSession, (void(*))DiskChangedCallback,     this);
        DAUnregisterCallback(m_daSession, (void(*))DiskDisappearedCallback, this);
        DAUnregisterCallback(m_daSession, (void(*))DiskAppearedCallback,    this);
        CFRelease(m_daSession);
    }
}

bool TorcStorageOSX::Mount(const QString &Disk)
{
    if (m_daSession)
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("Trying to mount '%1'").arg(Disk));

        QByteArray name = Disk.toLocal8Bit();
        DADiskRef disk = DADiskCreateFromBSDName(kCFAllocatorDefault, m_daSession, (const char*)name.data());
        DADiskMount(disk, NULL, kDADiskMountOptionWhole, DiskMountCallback, this);
        CFRelease(disk);

        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, "Not initialised");
    return false;
}

bool TorcStorageOSX::Unmount(const QString &Disk)
{
    if (m_daSession)
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("Trying to unmount '%1'").arg(Disk));

        QByteArray name = Disk.toLocal8Bit();
        DADiskRef disk = DADiskCreateFromBSDName(kCFAllocatorDefault, m_daSession, (const char*)name.data());
        DADiskUnmount(disk, kDADiskUnmountOptionForce, DiskUnmountCallback, this);
        CFRelease(disk);

        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, "Not initialised");
    return false;
}

bool TorcStorageOSX::Eject(const QString &Disk)
{
    if (m_daSession)
    {
        QByteArray name = Disk.toLocal8Bit();
        DADiskRef disk = DADiskCreateFromBSDName(kCFAllocatorDefault, m_daSession, (const char*)name.data());
        DADiskUnmount(disk, kDADiskUnmountOptionForce, DiskUnmountCallback, this);
        CFRelease(disk);

        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, "Not initialised");
    return false;
}

bool TorcStorageOSX::ReallyEject(const QString &Disk)
{
    if (m_daSession)
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("Trying to eject '%1'").arg(Disk));

        QByteArray name = Disk.toLocal8Bit();
        DADiskRef disk = DADiskCreateFromBSDName(kCFAllocatorDefault, m_daSession, (const char*)name.data());
        DADiskEject(disk, kDADiskEjectOptionDefault, DiskEjectCallback, this);
        CFRelease(disk);

        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, "Not initialised");
    return false;
}

void TorcStorageOSX::DiskAppearedCallback(DADiskRef Disk, void *Context)
{
    TorcStorageOSX *storage = static_cast<TorcStorageOSX*>(Context);
    if (storage)
        storage->AddDisk(Disk);
}

void TorcStorageOSX::DiskDisappearedCallback(DADiskRef Disk, void *Context)
{
    TorcStorageOSX *storage = static_cast<TorcStorageOSX*>(Context);
    if (storage)
        storage->RemoveDisk(Disk);
}

void TorcStorageOSX::DiskChangedCallback(DADiskRef Disk, CFArrayRef Keys, void *Context)
{
    TorcStorageOSX *storage = static_cast<TorcStorageOSX*>(Context);
    if (storage && CFArrayContainsValue(Keys, CFRangeMake(0, CFArrayGetCount(Keys)),
                                        kDADiskDescriptionVolumeNameKey))
    {
        storage->ChangeDisk(Disk);
    }
}

void TorcStorageOSX::DiskMountCallback(DADiskRef Disk, DADissenterRef Dissenter, void *Context)
{
    if (Dissenter)
    {
        CFStringRef reason = DADissenterGetStatusString(Dissenter);
        LOG(VB_GENERAL, LOG_INFO, QString("Failed to mount disk - reason '%1', code '%2'")
            .arg(CFStringReftoQString(reason))
            .arg(DADissenterGetStatus(Dissenter) & 0xffffffff, 0, 16));
    }
    else if (Disk)
    {
        // update master list
        TorcStorageOSX *storage = static_cast<TorcStorageOSX*>(Context);
        if (storage)
            storage->DiskMounted(Disk);
    }
}

void TorcStorageOSX::DiskUnmountCallback(DADiskRef Disk, DADissenterRef Dissenter, void *Context)
{
    if (Dissenter)
    {
        CFStringRef reason = DADissenterGetStatusString(Dissenter);
        LOG(VB_GENERAL, LOG_INFO, QString("Failed to unmount disk - reason '%1', code '%2'")
            .arg(CFStringReftoQString(reason))
            .arg(DADissenterGetStatus(Dissenter) & 0xffffffff, 0, 16));
    }
    else if (Disk)
    {
        // update master list
        TorcStorageOSX *storage = static_cast<TorcStorageOSX*>(Context);
        if (storage)
            storage->DiskUnmounted(Disk);
    }
}

void TorcStorageOSX::DiskEjectCallback(DADiskRef Disk, DADissenterRef Dissenter, void *Context)
{
    if (Dissenter)
    {
        CFStringRef reason = DADissenterGetStatusString(Dissenter);
        LOG(VB_GENERAL, LOG_INFO, QString("Failed to eject disk - reason '%1', code '%2'")
            .arg(CFStringReftoQString(reason))
            .arg(DADissenterGetStatus(Dissenter) & 0xffffffff, 0, 16));
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, "Disk ejected");
    }
}

TorcStorageDevice TorcStorageOSX::GetDiskDetails(DADiskRef Disk)
{
    TorcStorageDevice device;
    if (!Disk)
        return device;

    // get the bsd name
    const char* name = DADiskGetBSDName(Disk);
    device.SetSystemName(QString(name));

    // get the volume name
    CFDictionaryRef description = DADiskCopyDescription(Disk);
    if (description)
    {
        CFStringRef volumeref = (CFStringRef)CFDictionaryGetValue(description, kDADiskDescriptionVolumeNameKey);

        // name
        if (volumeref)
        {
            device.SetProperties(device.GetProperties() | TorcStorageDevice::Mounted);
            device.SetName("/Volumes/" + CFStringReftoQString(volumeref));
        }

        // removable
        bool removable = kCFBooleanTrue == CFDictionaryGetValue(description, kDADiskDescriptionMediaRemovableKey);
        device.SetType(removable ? TorcStorageDevice::RemovableDisk : TorcStorageDevice::FixedDisk);
        if (removable)
            device.SetProperties(device.GetProperties() | TorcStorageDevice::Removable);

        // writable
        bool writeable = kCFBooleanTrue == CFDictionaryGetValue(description, kDADiskDescriptionMediaWritableKey);
        if (writeable)
            device.SetProperties(device.GetProperties() | TorcStorageDevice::Writeable);

        // ejectable
        bool ejectable = kCFBooleanTrue == CFDictionaryGetValue(description, kDADiskDescriptionMediaEjectableKey);
        if (ejectable)
            device.SetProperties(device.GetProperties() | TorcStorageDevice::Ejectable);

        // description
        QString devicedescription;

        CFStringRef vendorref = (CFStringRef)CFDictionaryGetValue(description, kDADiskDescriptionDeviceVendorKey);
        if (vendorref)
            devicedescription = CFStringReftoQString(vendorref);

        CFStringRef modelref = (CFStringRef)CFDictionaryGetValue(description, kDADiskDescriptionDeviceModelKey);
        if (modelref)
            devicedescription += QString(" %1").arg(CFStringReftoQString(modelref));

        device.SetDescription(devicedescription.simplified());

        CFRelease(description);
    }

    // additional details
    CFMutableDictionaryRef dict = IOBSDNameMatching(kIOMasterPortDefault, 0, name);

    if (dict)
    {
        io_iterator_t ioiter;

        if (KERN_SUCCESS == IOServiceGetMatchingServices(kIOMasterPortDefault, dict, &ioiter))
        {
            if (ioiter)
            {
                io_service_t ioservice = IOIteratorNext(ioiter);
                if (ioservice)
                {
                    io_iterator_t serviceiterator;
                    if (KERN_SUCCESS == IORegistryEntryCreateIterator(ioservice,
                                            kIOServicePlane, kIORegistryIterateRecursively| kIORegistryIterateParents,
                                            &serviceiterator))
                    {
                        if (serviceiterator)
                        {
                            IOObjectRetain(ioservice);
                            bool found = false;

                            do
                            {
                                if (IOObjectConformsTo(ioservice, kIOMediaClass))
                                {
                                    CFTypeRef whole = IORegistryEntryCreateCFProperty(ioservice, CFSTR(kIOMediaWholeKey), kCFAllocatorDefault, 0);
                                    if (whole)
                                    {
                                        if (IOObjectConformsTo(ioservice, kIODVDMediaClass))
                                            device.SetType(TorcStorageDevice::DVD);
                                        else if (IOObjectConformsTo(ioservice, kIOCDMediaClass))
                                            device.SetType(TorcStorageDevice::CD);
                                        else if (IOObjectConformsTo(ioservice, kIOBDMediaClass))
                                            device.SetType(TorcStorageDevice::BD);

                                        CFRelease(whole);
                                    }
                                }

                            } while (!found && (ioservice = IOIteratorNext(serviceiterator)));

                            IOObjectRelease(serviceiterator);
                        }
                    }

                    IOObjectRelease(ioservice);
                }

                IOObjectRelease(ioiter);
            }
        }
    }

    return device;
}

void TorcStorageOSX::AddDisk(DADiskRef Disk)
{
    if (!Disk || !parent())
        return;

    TorcStorageDevice device = GetDiskDetails(Disk);
    ((TorcStorage*)parent())->AddDisk(device);
}

void TorcStorageOSX::RemoveDisk(DADiskRef Disk)
{
    if (!Disk || !parent())
        return;

    TorcStorageDevice device = GetDiskDetails(Disk);
    ((TorcStorage*)parent())->RemoveDisk(device);
}


void TorcStorageOSX::ChangeDisk(DADiskRef Disk)
{
    if (!Disk || !parent())
        return;

    TorcStorageDevice device = GetDiskDetails(Disk);
    ((TorcStorage*)parent())->ChangeDisk(device);
}

void TorcStorageOSX::DiskMounted(DADiskRef Disk)
{
    if (!Disk || !parent())
        return;

    TorcStorageDevice device = GetDiskDetails(Disk);
    ((TorcStorage*)parent())->DiskMounted(device);
}

void TorcStorageOSX::DiskUnmounted(DADiskRef Disk)
{
    if (!Disk || !parent())
        return;

    TorcStorageDevice disk = GetDiskDetails(Disk);
    ((TorcStorage*)parent())->DiskUnmounted(disk);
}
