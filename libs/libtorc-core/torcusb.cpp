/* Class TorcUSB
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
#include "torcconfig.h"
#include "torclocalcontext.h"
#include "torcadminthread.h"
#include "torcusb.h"

TorcUSBPriv* TorcUSBPriv::Create(TorcUSB *Parent)
{
    TorcUSBPriv *usb = NULL;

    int score = 0;
    USBFactory* factory = USBFactory::GetUSBFactory();
    for ( ; factory; factory = factory->NextFactory())
        (void)factory->Score(score);

    factory = USBFactory::GetUSBFactory();
    for ( ; factory; factory = factory->NextFactory())
    {
        usb = factory->Create(score, Parent);
        if (usb)
            break;
    }

    if (!usb)
        LOG(VB_GENERAL, LOG_ERR, "Failed to create usb implementation");

    return usb;
}

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

QVariantMap TorcUSBDevice::ToMap(void)
{
    QVariantMap result;
    result.insert("path",      m_path);
    result.insert("vendor",    m_vendor);
    result.insert("vendorid",  m_vendorID);
    result.insert("product",   m_product);
    result.insert("productid", m_productID);
    result.insert("class",     ClassToString(m_class));
    return result;
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
 * A singleton is created in the administration thread by TorcUSBObject. All
 * interaction with this object is via Qt events (see TorcLocalContext::NotifyEvent)
 *
 * \sa TorcUSBPriv
 * \sa TorcUSBObject
*/

TorcUSB::TorcUSB()
  : QObject(NULL),
    TorcHTTPService(this, "/usb", tr("USB"), TorcUSB::staticMetaObject),
    m_priv(NULL),
    m_managedDevicesLock(new QMutex())
{
    m_priv = TorcUSBPriv::Create(this);
    // listen for refresh events
    gLocalContext->AddObserver(this);
}

TorcUSB::~TorcUSB()
{
    // stop listening for refresh events
    gLocalContext->RemoveObserver(this);

    // delete implementation
    if (m_priv)
        m_priv->Destroy();

    // cleanup device handlers
    {
        QMutexLocker locker(m_managedDevicesLock);

        QMap<QString,TorcUSBDevice>::const_iterator it = m_managedDevices.begin();
        for ( ; it != m_managedDevices.end(); ++it)
            TorcUSBDeviceHandler::DeviceHandled(it.value(), false /*removed*/);
        m_managedDevices.clear();
    }

    // delete lock
    delete m_managedDevicesLock;
}

bool TorcUSB::event(QEvent *Event)
{
    if (Event && Event->type() == TorcEvent::TorcEventType)
    {
        TorcEvent* torcevent = dynamic_cast<TorcEvent*>(Event);
        if (torcevent && torcevent->GetEvent() == Torc::USBRescan && m_priv)
        {
            m_priv->Refresh();
            return true;
        }
    }

    return false;
}

QVariantMap TorcUSB::GetDevices(void)
{
    QMutexLocker locker(m_managedDevicesLock);
    QVariantMap result;
    QMap<QString,TorcUSBDevice>::iterator it = m_managedDevices.begin();
    for ( ; it != m_managedDevices.end(); ++it)
        result.insert(it.key(), it.value().ToMap());
    return result;
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
        m_managedDevices.insert(Device.m_path, Device);
    }
}

void TorcUSB::DeviceRemoved(const TorcUSBDevice &Device)
{
    LOG(VB_GENERAL, LOG_DEBUG, QString("Device removed: %1")
        .arg(Device.m_path));

    if (TorcUSBDeviceHandler::DeviceHandled(Device, false /*removed*/))
    {
        QMutexLocker locker(m_managedDevicesLock);
        while (m_managedDevices.contains(Device.m_path))
            (void)m_managedDevices.remove(Device.m_path);
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
        if (gLocalContext->FlagIsSet(Torc::USB))
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

class TorcUSBNull : public TorcUSBPriv
{
  public:
    void Destroy (void) {}
    void Refresh (void) {}
};

class USBFactoryNull : public USBFactory
{
    void Score(int &Score)
    {
        if (Score <= 1)
            Score = 1;
    }

    TorcUSBPriv* Create(int Score, TorcUSB *Parent)
    {
        (void)Parent;

        if (Score <= 1)
            return new TorcUSBNull();

        return NULL;
    }
} USBFactoryNull;

USBFactory* USBFactory::gUSBFactory = NULL;

USBFactory::USBFactory()
{
    nextUSBFactory = gUSBFactory;
    gUSBFactory = this;
}

USBFactory::~USBFactory()
{
}

USBFactory* USBFactory::GetUSBFactory(void)
{
    return gUSBFactory;
}

USBFactory* USBFactory::NextFactory(void) const
{
    return nextUSBFactory;
}

