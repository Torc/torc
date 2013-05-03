/* Class TorcStorageUnixDBus
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
    QDBusInterface interface("org.freedesktop.UDisks", Disk,
                             "org.freedesktop.UDisks.Device",
                             QDBusConnection::systemBus());

    // this needs an fstab entry
    QList<QVariant> args;
    args << QString("");
    args << QStringList();
    args << QString("");

    if (interface.callWithCallback(QLatin1String("FilesystemMount"), args, (QObject*)this, SLOT(DBusCallback()), SLOT(DBusError(QDBusError))))
        return true;

    LOG(VB_GENERAL, LOG_ERR, "Mount call failed");
    return false;
}

bool TorcStorageUnixDBus::Unmount(const QString &Disk)
{
    QDBusInterface interface("org.freedesktop.UDisks", Disk,
                             "org.freedesktop.UDisks.Device",
                             QDBusConnection::systemBus());

    QStringList arg;
    arg << QString("force");
    QList<QVariant> args;
    args << arg;

    if (interface.callWithCallback(QLatin1String("FilesystemUnmount"), args,(QObject*)this, SLOT(DBusCallback()), SLOT(DBusError(QDBusError))))
        return true;

    LOG(VB_GENERAL, LOG_ERR, "Unmount call failed");
    return false;
}

bool TorcStorageUnixDBus::Eject(const QString &Disk)
{
    return Unmount(Disk);
}

bool TorcStorageUnixDBus::ReallyEject(const QString &Disk)
{
    LOG(VB_GENERAL, LOG_DEBUG, "Trying to eject " + Disk);

    QDBusInterface interface("org.freedesktop.UDisks", Disk,
                             "org.freedesktop.UDisks.Device",
                             QDBusConnection::systemBus());

    QList<QVariant> args;
    args << QStringList();

    if (interface.callWithCallback(QLatin1String("DriveEject"), args,(QObject*)this, SLOT(DBusCallback()), SLOT(DBusError(QDBusError))))
        return true;

    LOG(VB_GENERAL, LOG_ERR, "Eject call failed");
    return false;
}

void TorcStorageUnixDBus::DiskAdded(QDBusObjectPath Path)
{
    if (!Path.path().isEmpty())
    {
        TorcStorageDevice device = GetDiskDetails(Path.path());
        ((TorcStorage*)parent())->AddDisk(device);
    }
}

void TorcStorageUnixDBus::DiskRemoved(QDBusObjectPath Path)
{
    if (!Path.path().isEmpty())
    {
        TorcStorageDevice device = GetDiskDetails(Path.path());
        device.SetSystemName(Path.path());
        ((TorcStorage*)parent())->RemoveDisk(device);
    }
}

void TorcStorageUnixDBus::DiskChanged(QDBusObjectPath Path)
{
    if (!Path.path().isEmpty())
    {
        TorcStorageDevice device = GetDiskDetails(Path.path());
        ((TorcStorage*)parent())->ChangeDisk(device);
    }
}

TorcStorageDevice TorcStorageUnixDBus::GetDiskDetails(const QString &Disk)
{
    TorcStorageDevice device;
    if (Disk.isEmpty())
        return device;

    device.SetSystemName(Disk);

    QDBusInterface interface("org.freedesktop.UDisks", Disk,
                             "org.freedesktop.UDisks.Device",
                             QDBusConnection::systemBus());

    if (interface.isValid())
    {
        QVariant partition = interface.property("DeviceIsPartition");
        QVariant table     = interface.property("DeviceIsPartitionTable");
        QVariant internal  = interface.property("DeviceIsSystemInternal");
        QVariant name      = interface.property("DeviceFile");
        QVariant mounted   = interface.property("DeviceIsMounted");
        QVariant usage     = interface.property("IdUsage");
        QVariant optical   = interface.property("DeviceIsOpticalDisc");
        QVariant removable = interface.property("DeviceIsRemovable");
        QVariant ejectable = interface.property("DriveIsMediaEjectable");
        QVariant mediatype = interface.property("DriveMedia");
        QVariant medialist = interface.property("DriveMediaCompatibility");
        QVariant readonly  = interface.property("DeviceIsReadOnly");
        QVariant vendor    = interface.property("DriveVendor");
        QVariant model     = interface.property("DriveModel");
        QVariant mounts    = interface.property("DeviceMountPaths");

        if (table.isValid() && table.toBool() == true)
        {
            LOG(VB_GENERAL, LOG_DEBUG, "Ignoring partition table");
            return device;
        }

#if 0
        LOG(VB_GENERAL, LOG_INFO, QString("DeviceIsPartition       : %1").arg(partition.toString()));
        LOG(VB_GENERAL, LOG_INFO, QString("DeviceIsPartitionTable  : %1").arg(table.toString()));
        LOG(VB_GENERAL, LOG_INFO, QString("DeviceIsSystemInternal  : %1").arg(internal.toString()));
        LOG(VB_GENERAL, LOG_INFO, QString("DeviceFile              : %1").arg(name.toString()));
        LOG(VB_GENERAL, LOG_INFO, QString("DeviceIsMounted         : %1").arg(mounted.toString()));
        LOG(VB_GENERAL, LOG_INFO, QString("IdUsage                 : %1").arg(usage.toString()));
        LOG(VB_GENERAL, LOG_INFO, QString("DeviceIsOpticalDisc     : %1").arg(optical.toString()));
        LOG(VB_GENERAL, LOG_INFO, QString("DeviceIsRemovable       : %1").arg(removable.toString()));
        LOG(VB_GENERAL, LOG_INFO, QString("DriveIsMediaEjectable   : %1").arg(ejectable.toString()));
        LOG(VB_GENERAL, LOG_INFO, QString("DriveMedia              : %1").arg(mediatype.toString()));
        LOG(VB_GENERAL, LOG_INFO, QString("DeviceIsReadOnly        : %1").arg(readonly.toString()));
        LOG(VB_GENERAL, LOG_INFO, QString("DriveVendor             : %1").arg(vendor.toString()));
        LOG(VB_GENERAL, LOG_INFO, QString("DriveModel              : %1").arg(model.toString()));

        if (mounts.isValid())
            foreach(QString mount, mounts.toStringList())
                LOG(VB_GENERAL, LOG_INFO, QString("Mount path: %1").arg(mount));

        if (medialist.isValid())
            foreach(QString media, medialist.toStringList())
                LOG(VB_GENERAL, LOG_INFO, QString("Media type: %1").arg(media));
#endif

        if (mounted.isValid() && mounted.toBool() == true)
        {
            QStringList mountlist;
            if (mounts.isValid())
                mountlist = mounts.toStringList();

            if (!mountlist.isEmpty())
            {
                device.SetName(mountlist[0]);
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, "Device is marked as mounted but has no mount point");
            }

            device.SetProperties(device.GetProperties() | TorcStorageDevice::Mounted);
        }

        // optical disks...
        if (medialist.isValid() && !medialist.toStringList().isEmpty())
        {
            QString media = mediatype.isValid() ? mediatype.toString() : QString("");
            TorcStorageDevice::StorageType disktype = TorcStorageDevice::Optical;

            if (!media.isEmpty())
            {
                if (media.contains("hddvd"))
                    disktype = TorcStorageDevice::HDDVD;
                else if (media.contains("dvd"))
                    disktype = TorcStorageDevice::DVD;
                else if (media.contains("cd"))
                    disktype = TorcStorageDevice::CD;
                else if (media.contains("bd"))
                    disktype = TorcStorageDevice::BD;
            }

            device.SetType(disktype);
            device.SetProperties(device.GetProperties() | TorcStorageDevice::Removable);
        }
        else
        {
            // NB this may be problematic as various properties (removable etc)
            // seem to be unreliable
            if (internal.isValid() && internal.toBool() == false)
            {
                device.SetType(TorcStorageDevice::RemovableDisk);
                device.SetProperties(device.GetProperties() | TorcStorageDevice::Removable);
            }
            else
            {
                if (usage.isValid() && usage.toString() == "filesystem")
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

void TorcStorageUnixDBus::DBusError(QDBusError Error)
{
    LOG(VB_GENERAL, LOG_ERR, QString("DBus callback error: %1, %2")
        .arg(Error.name()).arg(Error.message().trimmed()));
}

void TorcStorageUnixDBus::DBusCallback(void)
{
}
