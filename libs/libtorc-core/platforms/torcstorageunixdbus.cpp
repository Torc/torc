/* Class TorcStorageUnixDBus
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
#include "torcstoragedevice.h"
#include "torcstorageunixdbus.h"

/*! \class TorcStorageUnixDBus
 *  \brief A class for detecting disk drives on Unix based systems.
 *
 * TorcStorageUnixDBus uses the UDisks DBus interfaces to scan for available storage devices
 * and monitor for changes as removable devices are inserted and ejected.
*/

TorcStorageUnixDBus::TorcStorageUnixDBus(TorcStorage *Parent)
  : TorcStoragePriv(Parent)
{
    // create interface
    m_udisksInterface = new QDBusInterface("org.freedesktop.UDisks",
                                           "/org/freedesktop/UDisks",
                                           "org.freedesktop.UDisks",
                                           QDBusConnection::systemBus());

    if (m_udisksInterface)
    {
        if (!m_udisksInterface->isValid())
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to create UDisks interface");
            delete m_udisksInterface;
            m_udisksInterface = NULL;
        }
    }

    if (m_udisksInterface)
    {
        // enumerate disks
        QDBusReply<QList<QDBusObjectPath> > diskscheck = m_udisksInterface->call("EnumerateDevices");
        if (diskscheck.isValid())
        {
            foreach(QDBusObjectPath path, diskscheck.value())
            {
                TorcStorageDevice device = GetDiskDetails(path.path());
                ((TorcStorage*)parent())->AddDisk(device);
            }
        }

        // listen for changes
        if (!QDBusConnection::systemBus().connect(
             "org.freedesktop.UDisks", "/org/freedesktop/UDisks",
             "org.freedesktop.UDisks", "DeviceAdded", "o",
             this, SLOT(DiskAdded(QDBusObjectPath))))
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to register for DiskAdded");
        }

        if (!QDBusConnection::systemBus().connect(
             "org.freedesktop.UDisks", "/org/freedesktop/UDisks",
             "org.freedesktop.UDisks", "DeviceRemoved", "o",
             this, SLOT(DiskRemoved(QDBusObjectPath))))
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to register for DiskRemoved");
        }

        if (!QDBusConnection::systemBus().connect(
             "org.freedesktop.UDisks", "/org/freedesktop/UDisks",
             "org.freedesktop.UDisks", "DeviceChanged", "o",
             this, SLOT(DiskChanged(QDBusObjectPath))))
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to register for DiskChanged");
        }
    }
}

TorcStorageUnixDBus::~TorcStorageUnixDBus()
{
    delete m_udisksInterface;
}

bool TorcStorageUnixDBus::Mount(const QString &Disk)
{
    return false;
}

bool TorcStorageUnixDBus::Unmount(const QString &Disk)
{
    return false;
}

bool TorcStorageUnixDBus::Eject(const QString &Disk)
{
    return false;
}

void TorcStorageUnixDBus::DiskAdded(QDBusObjectPath Path)
{
    LOG(VB_GENERAL, LOG_INFO, Path.path());
}

void TorcStorageUnixDBus::DiskRemoved(QDBusObjectPath Path)
{
    LOG(VB_GENERAL, LOG_INFO, Path.path());
}

void TorcStorageUnixDBus::DiskChanged(QDBusObjectPath Path)
{
    LOG(VB_GENERAL, LOG_INFO, Path.path());
}

TorcStorageDevice TorcStorageUnixDBus::GetDiskDetails(const QString &Disk)
{
    LOG(VB_GENERAL, LOG_INFO, "-------------------------------");

    TorcStorageDevice device;
    device.SetSystemName(Disk);

    if (Disk.isEmpty())
        return device;

    QDBusInterface interface("org.freedesktop.UDisks", Disk,
                             "org.freedesktop.UDisks.Device",
                             QDBusConnection::systemBus());

    if (interface.isValid())
    {
        QVariant partition = interface.property("DeviceIsPartition");
        QVariant name      = interface.property("DeviceFile");
        QVariant mounted   = interface.property("DeviceIsMounted");
        QVariant optical   = interface.property("DeviceIsOpticalDisc");
        QVariant removable = interface.property("DeviceIsRemovable");
        QVariant ejectable = interface.property("DriveIsMediaEjectable");
        QVariant mediatype = interface.property("DriveMedia");
        QVariant medialist = interface.property("DriveMediaCompatibility");
        QVariant readonly  = interface.property("DeviceIsReadOnly");
        QVariant vendor    = interface.property("DriveVendor");
        QVariant model     = interface.property("DriveModel");
        QVariant mounts    = interface.property("DeviceMountPaths");

        if (mounts.isValid())
        {
            foreach(QString mount, mounts.toStringList())
                LOG(VB_GENERAL, LOG_INFO, mount);
        }


        if (medialist.isValid())
        {
            foreach(QString media, medialist.toStringList())
                LOG(VB_GENERAL, LOG_INFO, media);
        }

        if (name.isValid())
            device.SetName(name.toString());

        if (mounted.isValid() && mounted.toBool() == true)
            device.SetProperties(device.GetProperties() | TorcStorageDevice::Mounted);

        if (optical.isValid() && optical.toBool() == true)
        {
            LOG(VB_GENERAL, LOG_INFO, "optical");
            if (mediatype.isValid())
            {
                QString type = mediatype.toString();
                LOG(VB_GENERAL, LOG_INFO, type);
                if (type.contains("optical_cd"))
                    device.SetType(TorcStorageDevice::CD);
                else if (type.contains("optical_dvd"))
                    device.SetType(TorcStorageDevice::DVD);
                else if (type.contains("optical_bd"))
                    device.SetType(TorcStorageDevice::BD);

                device.SetProperties(device.GetProperties() | TorcStorageDevice::Removable);
            }
        }
        else
        {
            if (removable.isValid() && removable.toBool() == true)
            {
                device.SetType(TorcStorageDevice::RemovableDisk);
                device.SetProperties(device.GetProperties() | TorcStorageDevice::Removable);
            }
            else
            {
                device.SetType(TorcStorageDevice::FixedDisk);
            }
        }

        if (ejectable.isValid() && ejectable.toBool() == true)
            device.SetProperties(device.GetProperties() | TorcStorageDevice::Ejectable);

        if (readonly.isValid() && readonly.toBool() == false)
            device.SetProperties(device.GetProperties() | TorcStorageDevice::Writeable);

        QString description;

        if (vendor.isValid())
            description = vendor.toString();

        if (model.isValid())
            description += QString(" %1").arg(model.toString());

        device.SetDescription(description);


        return device;
    }

    LOG(VB_GENERAL, LOG_ERR, QString("Failed to create UDisks.Device interface for '%1'").arg(Disk));
    return device;
}
