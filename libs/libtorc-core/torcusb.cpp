/* Class TorcUSB
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
#include "torcusb.h"

#ifdef Q_OS_MAC
#include "torcusbprivosx.h"
#elif CONFIG_LIBUDEV
#include "torcusbprivunix.h"
#endif

TorcUSBDevice::TorcUSBDevice()
    : m_path(QString("")),
      m_vendorID(-1),
      m_productID(-1),
      m_class(Unknown)
{
}

TorcUSBDevice::TorcUSBDevice(const QString &Path, int Vendor, int Product, TorcUSBDevice::Classes Class)
    : m_path(Path),
      m_vendorID(Vendor),
      m_productID(Product),
      m_class(Class)
{
}

QString TorcUSBDevice::ClassToString(Classes Class)
{
    switch (Class)
    {
        case PerInterface: return "Composite";
        case Audio:        return "Audio";
        case Comm:         return "Communications";
        case HID:          return "HID";
        case Physical:     return "Physical";
        case Still:        return "Still";
        case Printer:      return "Printer";
        case MassStorage:  return "Mass Storage";
        case Hub:          return "Hub";
        case Data:         return "Data";
        case AppSpec:      return "AppSpec";
        case VendorSpec:   return "VendorSpec";
        default:           break;
    }

    return "Unknown";
}

bool TorcUSBDevice::IgnoreClass(Classes Class)
{
    if (MassStorage == Class ||
        Hub         == Class ||
        Printer     == Class)
    {
        return true;
    }

    return false;
}

TorcUSBDeviceHandler* TorcUSBDeviceHandler::gUSBDeviceHandler = NULL;
int TorcUSBDeviceHandler::gDevicesSeen = TorcUSBDeviceHandler::Unknown;

TorcUSBDeviceHandler::TorcUSBDeviceHandler()
{
    m_nextUSBDeviceHandler = gUSBDeviceHandler;
    gUSBDeviceHandler = this;
}

TorcUSBDeviceHandler::~TorcUSBDeviceHandler()
{
}

bool TorcUSBDeviceHandler::DeviceHandled(const TorcUSBDevice &Device, bool Added)
{
    static QMutex lock;
    QMutexLocker locker(&lock);

    TorcUSBDeviceHandler *handler = gUSBDeviceHandler;
    for ( ; handler; handler = handler->GetNextHandler())
    {
        if (Added)
        {
            if (handler->DeviceAdded(Device))
                return true;
        }
        else
        {
            if (handler->DeviceRemoved(Device))
                return true;
        }
    }

    // register known devices that setup may be interesed in
    if (Device.m_vendorID == 0x22b8 && Device.m_productID == 0x003b)
    {
        if (Added)
            gDevicesSeen = gDevicesSeen | Nyxboard;
        else
            gDevicesSeen = gDevicesSeen & ~Nyxboard;
    }

    return false;
}

bool TorcUSBDeviceHandler::DeviceSeen(KnownDevices Device)
{
    return gDevicesSeen & Device;
}

TorcUSBDeviceHandler* TorcUSBDeviceHandler::GetNextHandler(void)
{
    return m_nextUSBDeviceHandler;
}

/*! \class TorcUSB
 *  \brief A class to scan for new and removed USB devices.
 *
 * TorcUSB uses underlying platform implementations to monitor for USB devices
 * and calls the appropriate handler for each recognised device.
 *
 * \sa TorcUSBPriv
*/

TorcUSB::TorcUSB()
  : QObject(NULL),
    m_priv(NULL),
    m_managedDevicesLock(new QMutex())
{
#ifdef Q_OS_MAC
    m_priv = new TorcUSBPriv(this);
#elif CONFIG_LIBUDEV
    m_priv = new TorcUSBPriv(this);
#endif
}

TorcUSB::~TorcUSB()
{
    // delete implementation
    if (m_priv)
        m_priv->deleteLater();

    // cleanup device handlers
    {
        QMutexLocker locker(m_managedDevicesLock);

        foreach (QString path, m_managedDevices)
        {
            TorcUSBDevice device;
            device.m_path = path;
            TorcUSBDeviceHandler::DeviceHandled(device, false /*removed*/);
        }

        m_managedDevices.clear();
    }

    // delete lock
    delete m_managedDevicesLock;
}

void TorcUSB::DeviceAdded(const TorcUSBDevice &Device)
{
    LOG(VB_GENERAL, LOG_DEBUG, QString("New device: %1 %2:%3 (%4, %5, %6)")
        .arg(Device.m_path)
        .arg(Device.m_vendorID, 0, 16)
        .arg(Device.m_productID, 0, 16)
        .arg(TorcUSBDevice::ClassToString(Device.m_class))
        .arg(Device.m_product)
        .arg(Device.m_vendor));

    if (TorcUSBDeviceHandler::DeviceHandled(Device, true /*added*/))
    {
        QMutexLocker locker(m_managedDevicesLock);
        m_managedDevices.append(Device.m_path);
    }
}

void TorcUSB::DeviceRemoved(const TorcUSBDevice &Device)
{
    LOG(VB_GENERAL, LOG_DEBUG, QString("Device removed: %1")
        .arg(Device.m_path));

    if (TorcUSBDeviceHandler::DeviceHandled(Device, false /*removed*/))
    {
        QMutexLocker locker(m_managedDevicesLock);
        (void)m_managedDevices.removeAll(Device.m_path);
    }

}

/*! \class TorcUSBObject
 *  \brief A static class used to create a TorcUSB object in the admin thread.
*/
static class TorcUSBObject : public TorcAdminObject
{
  public:
    TorcUSBObject()
      : TorcAdminObject(TORC_ADMIN_MED_PRIORITY),
        m_usb(NULL)
    {
    }

    ~TorcUSBObject()
    {
        Destroy();
    }

    void Create(void)
    {
        Destroy();
        m_usb = new TorcUSB();
    }

    void Destroy(void)
    {
        delete m_usb;
        m_usb = NULL;
    }

  private:
    TorcUSB *m_usb;

} TorcUSBObject;

