/* Class TorcUSBPriv
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
#include "torcusbprivunix.h"

TorcUSBPriv::TorcUSBPriv(TorcUSB *Parent)
  : QObject(Parent),
    m_udev(NULL),
    m_udevMonitor(NULL),
    m_udevFD(-1),
    m_socketNotifier(NULL)
{
    // create udev connection
    m_udev = udev_new();

    // listen for updates
    if (m_udev)
    {
        m_udevMonitor = udev_monitor_new_from_netlink(m_udev, "udev");
        udev_monitor_enable_receiving(m_udevMonitor);
        m_udevFD = udev_monitor_get_fd(m_udevMonitor);

        if (m_udevFD > -1)
        {
            m_socketNotifier = new QSocketNotifier(m_udevFD, QSocketNotifier::Read, this);
            m_socketNotifier->setEnabled(true);
            QObject::connect(m_socketNotifier, SIGNAL(activated(int)),
                             this, SLOT(SocketReadyRead(int)));
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to initialise libudev");
    }

    // enumerate existing devices
    if (m_udev)
    {
        udev_enumerate *enumerator = udev_enumerate_new(m_udev);
        udev_enumerate_scan_devices(enumerator);
        udev_list_entry *devices = udev_enumerate_get_list_entry(enumerator);
        udev_list_entry *entry;

        udev_list_entry_foreach(entry, devices)
        {
            if (!udev_list_entry_get_name(entry))
                continue;

            udev_device *device = udev_device_new_from_syspath(m_udev, udev_list_entry_get_name(entry));
            if (device)
            {
                AddDevice(device);
                udev_device_unref(device);
            }
        }

        udev_enumerate_unref(enumerator);
    }
}

TorcUSBPriv::~TorcUSBPriv()
{
    // stop updates
    if (m_socketNotifier)
        m_socketNotifier->setEnabled(false);

    // stop monitoring
    if (m_udevMonitor)
        udev_monitor_unref(m_udevMonitor);
    m_udevMonitor = NULL;

    // close connection
    if (m_udev)
        udev_unref(m_udev);
    m_udev = NULL;

    // delete notifier
    if (m_socketNotifier)
        m_socketNotifier->deleteLater();
    m_socketNotifier = NULL;
}

TorcUSBDevice TorcUSBPriv::GetDevice(udev_device *Device, bool Remove)
{
    if (Device && udev_device_get_subsystem(Device))
    {
        QString bus = QString::fromLocal8Bit(udev_device_get_property_value(Device, "ID_BUS"));
        udev_device *parentdevice;

        if (bus == "usb" && (parentdevice = udev_device_get_parent(udev_device_get_parent(Device))))
        {
#if 0
            struct udev_list_entry *entry;
            udev_list_entry_foreach(entry, udev_device_get_properties_list_entry(Device))
            {
                LOG(VB_GENERAL, LOG_INFO, QString("%1=%2")
                    .arg(QString::fromLocal8Bit(udev_list_entry_get_name(entry)))
                    .arg(QString::fromLocal8Bit(udev_list_entry_get_value(entry))));
            }
#endif

            QString classs = QString::fromLocal8Bit(udev_device_get_sysattr_value(parentdevice, "bDeviceClass"));

            bool classok;
            int classi  = classs.toInt(&classok, 16);
            if (!classok || classs.isEmpty())
                classi = 0;

            TorcUSBDevice::Classes torcclass = ToTorcClass(classi);
            if (!TorcUSBDevice::IgnoreClass(torcclass) || Remove)
            {
                QString path         = udev_device_get_syspath(Remove ? Device : Device /*parentdevice*/);
                QString vendors      = QString::fromLocal8Bit(udev_device_get_property_value(Device, "ID_VENDOR_ID"));
                QString products     = QString::fromLocal8Bit(udev_device_get_property_value(Device, "ID_MODEL_ID"));
                QString manufacturer = QString::fromLocal8Bit(udev_device_get_property_value(Device, "ID_VENDOR"));
                QString productdesc  = QString::fromLocal8Bit(udev_device_get_property_value(Device, "ID_MODEL"));

                bool vendorok;
                bool productok;
                int vendor  = vendors.toInt(&vendorok, 16);
                int product = products.toInt(&productok, 16);
                if (!vendorok || vendors.isEmpty())
                    vendor = 0;
                if (!productok || products.isEmpty())
                    product = 0;

                TorcUSBDevice device(path, vendor, product, torcclass);
                device.m_vendor  = manufacturer;
                device.m_product = productdesc;
                return device;
            }
        }
    }

    TorcUSBDevice dummy;
    return dummy;
}

void TorcUSBPriv::AddDevice(udev_device *Device)
{
    if (!Device)
        return;

    TorcUSBDevice device = GetDevice(Device);

    if (device.m_class != TorcUSBDevice::Unknown && device.m_productID && device.m_vendorID)
        ((TorcUSB*)parent())->DeviceAdded(device);
}

void TorcUSBPriv::RemoveDevice(udev_device *Device)
{
    if (!Device)
        return;

    TorcUSBDevice device = GetDevice(Device, true);

    if (device.m_class != TorcUSBDevice::Unknown && device.m_productID && device.m_vendorID)
        ((TorcUSB*)parent())->DeviceRemoved(device);
}

void TorcUSBPriv::ChangeDevice(udev_device *Device)
{
    LOG(VB_GENERAL, LOG_NOTICE, "libudev device changed");
}

void TorcUSBPriv::MoveDevice(udev_device *Device)
{
    LOG(VB_GENERAL, LOG_NOTICE, "libudev device moved");
}

void TorcUSBPriv::SocketReadyRead(int Device)
{
    if (Device == m_udevFD)
    {
        struct udev_device *device = udev_monitor_receive_device(m_udevMonitor);

        if (device)
        {
            QString action = QString::fromLocal8Bit(udev_device_get_action(device));

            if ("add" == action)
                AddDevice(device);
            else if ("remove" == action)
                RemoveDevice(device);
            else if ("change" == action)
                ChangeDevice(device);
            else if ("move" == action)
                MoveDevice(device);

            udev_device_unref(device);
        }
    }
}

enum TorcUSBDevice::Classes TorcUSBPriv::ToTorcClass(int UdevClass)
{
    // these should be a straight mapping...
    switch (UdevClass)
    {
        case 0x00: return TorcUSBDevice::PerInterface;
        case 0x01: return TorcUSBDevice::Audio;
        case 0x02: return TorcUSBDevice::Comm;
        case 0x03: return TorcUSBDevice::HID;
        case 0x05: return TorcUSBDevice::Physical;
        case 0x06: return TorcUSBDevice::Still;
        case 0x07: return TorcUSBDevice::Printer;
        case 0x08: return TorcUSBDevice::MassStorage;
        case 0x09: return TorcUSBDevice::Hub;
        case 0x0a: return TorcUSBDevice::Data;
        case 0xfe: return TorcUSBDevice::AppSpec;
        case 0xff: return TorcUSBDevice::VendorSpec;
    }

    return TorcUSBDevice::Unknown;
}
