/* Class TorcUSBPrivOSX
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
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOCFPlugIn.h>

// Torc
#include "torclogging.h"
#include "torcosxutils.h"
#include "torcrunlooposx.h"
#include "torcusbprivosx.h"

TorcUSBPrivOSX::TorcUSBPrivOSX(TorcUSB *Parent)
  : QObject(Parent),
    m_usbRef(NULL),
    m_usbNotifyPort(NULL),
    m_iterator(NULL),
    m_notificationsLock(new QMutex())
{
    if (!gAdminRunLoop)
    {
        LOG(VB_GENERAL, LOG_ERR, "OS X callback run loop not present - aborting");
        return;
    }

    CFMutableDictionaryRef match = IOServiceMatching(kIOUSBDeviceClassName);

    m_usbNotifyPort = IONotificationPortCreate(kIOMasterPortDefault);
    m_usbRef = IONotificationPortGetRunLoopSource(m_usbNotifyPort);
    CFRunLoopAddSource(gAdminRunLoop, m_usbRef, kCFRunLoopCommonModes);

    IOReturn ok = IOServiceAddMatchingNotification(m_usbNotifyPort, kIOFirstMatchNotification, match,
                                                   (IOServiceMatchingCallback)DeviceAddedCallback,
                                                   this, &m_iterator);
    if (kIOReturnSuccess == ok)
        DeviceAddedCallback(this, m_iterator);
    else
        LOG(VB_GENERAL, LOG_ERR, "Failed to register for attachment notifications");
}

TorcUSBPrivOSX::~TorcUSBPrivOSX()
{
    // free outstanding notifications
    LOG(VB_GENERAL, LOG_DEBUG, QString("%1 outstanding notifications").arg(m_notifications.size()));

    {
        QMutexLocker locker(m_notificationsLock);
        QMap<io_service_t,QPair<io_object_t,QString> >::iterator it = m_notifications.begin();
        for ( ; it != m_notifications.end(); ++it)
        {
            QPair<io_object_t,QString> pair = it.value();
            IOObjectRelease(pair.first);
        }
        m_notifications.clear();
    }

    // release iterator
    if (m_iterator)
        IOObjectRelease(m_iterator);
    m_iterator = NULL;

    // release source
    if (m_usbRef && gAdminRunLoop)
    {
        CFRunLoopRemoveSource(gAdminRunLoop, m_usbRef, kCFRunLoopDefaultMode);
        CFRelease(m_usbRef);
        m_usbRef = NULL;
    }

    // release notify port
    if (m_usbNotifyPort)
    {
        IONotificationPortDestroy(m_usbNotifyPort);
        m_usbNotifyPort = NULL;
    }

    // delete lock
    delete m_notificationsLock;
    m_notificationsLock = NULL;
}

void TorcUSBPrivOSX::Destroy(void)
{
    deleteLater();
}

void TorcUSBPrivOSX::Refresh(void)
{
}

TorcUSBDevice::Classes TorcUSBPrivOSX::ToTorcClass(int ClassType)
{
    switch (ClassType)
    {
    case kUSBCompositeClass:           return TorcUSBDevice::PerInterface;
    case kUSBCommunicationClass:       return TorcUSBDevice::Comm;
    case kUSBHubClass:                 return TorcUSBDevice::Hub;
    case kUSBDataClass:                return TorcUSBDevice::Data;
    //case kUSBPersonalHealthcareClass = 15,
    //case kUSBDiagnosticClass         = 220,
    //case kUSBWirelessControllerClass = 224,
    //case kUSBMiscellaneousClass      = 239,
    case kUSBApplicationSpecificClass: return TorcUSBDevice::AppSpec;
    case kUSBVendorSpecificClass:      return TorcUSBDevice::VendorSpec;
    }

    return TorcUSBDevice::Unknown;
}

void TorcUSBPrivOSX::AddDevice(TorcUSBDevice &Device, io_service_t Service,
                            io_object_t Notification)
{
    bool found = false;

    {
        QMutexLocker locker(m_notificationsLock);
        found = m_notifications.contains(Service);
    }

    if (!found)
    {
        {
            QMutexLocker locker(m_notificationsLock);
            QPair<io_object_t,QString> pair(Notification, Device.m_path);
            m_notifications.insert(Service, pair);
        }

        ((TorcUSB*)parent())->DeviceAdded(Device);
    }
    else
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("Device %1 already registered")
            .arg(Device.m_path));
        IOObjectRelease(Notification);
    }
}

IONotificationPortRef TorcUSBPrivOSX::GetNotificationPort(void)
{
    return m_usbNotifyPort;
}

TorcUSBDevice TorcUSBPrivOSX::GetDevice(io_service_t Device)
{
    TorcUSBDevice usbdevice;

    if (Device)
    {
        io_name_t buffer;
        IOReturn ok = IORegistryEntryGetName(Device, buffer);

        QString name = kIOReturnSuccess == ok ?  QString(buffer) : "Unknown";

        IOCFPlugInInterface **plugin;
        qint32 score;

        ok = IOCreatePlugInInterfaceForService(Device, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plugin, &score);
        if (kIOReturnSuccess == ok)
        {
            IOUSBDeviceInterface **interface;

            ok = (*plugin)->QueryInterface(plugin, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID), (void**)&interface);
            if (kIOReturnSuccess == ok)
            {
                quint16  vendor;
                quint16  product;
                quint32  location;
                quint8   classtype;

                IOReturn vendorok    = (*interface)->GetDeviceVendor(interface, &vendor);
                IOReturn productok   = (*interface)->GetDeviceProduct(interface, &product);
                IOReturn locationok  = (*interface)->GetLocationID(interface, &location);
                IOReturn classtypeok = (*interface)->GetDeviceClass(interface, &classtype);

                if ((classtypeok == kIOReturnSuccess) && !TorcUSBDevice::IgnoreClass(ToTorcClass(classtype)))
                {
                    TorcUSBDevice::Classes torcclass = ToTorcClass(classtype);

                    io_service_t usbinterface;
                    io_iterator_t iterator;
                    IOUSBFindInterfaceRequest request;
                    request.bInterfaceClass    = kIOUSBFindInterfaceDontCare;
                    request.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
                    request.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
                    request.bAlternateSetting  = kIOUSBFindInterfaceDontCare;

                    ok = (*interface)->CreateInterfaceIterator(interface, &request, &iterator);

                    while ((ok == kIOReturnSuccess) && (usbinterface = IOIteratorNext(iterator)))
                    {
                        IOCFPlugInInterface **interfaceplugin;
                        if (kIOReturnSuccess == IOCreatePlugInInterfaceForService(usbinterface, kIOUSBInterfaceUserClientTypeID,
                                                                                  kIOCFPlugInInterfaceID, &interfaceplugin, &score))
                        {
                            IOUSBInterfaceInterface** interfaceinterface;
                            if (kIOReturnSuccess == (*interfaceplugin)->QueryInterface(interfaceplugin, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID),
                                                                        (void**)&interfaceinterface))
                            {
                                QString path = locationok == kIOReturnSuccess ? QString::number(location) : "Error";

                                quint8 interfaceclass;
                                IOReturn result = (*interfaceinterface)->GetInterfaceClass(interfaceinterface, &interfaceclass);

                                if (result == kIOReturnSuccess)
                                {
                                    io_registry_entry_t parent;
                                    kern_return_t kernresult;
                                    kernresult = IORegistryEntryGetParentEntry(usbinterface, kIOServicePlane, &parent);
                                    if (kernresult == KERN_SUCCESS)
                                    {
                                        CFStringRef pathstring = (CFStringRef)IORegistryEntrySearchCFProperty(parent,
                                                kIOServicePlane, CFSTR(kIOCalloutDeviceKey), kCFAllocatorDefault, kIORegistryIterateRecursively);
                                        if (pathstring)
                                            path = CFStringReftoQString(pathstring);
                                        IOObjectRelease(parent);
                                    }
                                }

                                usbdevice = TorcUSBDevice(path, vendorok == kIOReturnSuccess ? vendor : 0,
                                                          productok == kIOReturnSuccess ? product : 0, torcclass);
                                usbdevice.m_product = name;
                            }

                            IODestroyPlugInInterface(interfaceplugin);
                        }

                        IOObjectRelease(usbinterface);
                    }
                }
            }

            IODestroyPlugInInterface(plugin);
        }
    }

    return usbdevice;
}

void TorcUSBPrivOSX::DeviceAddedCallback(void *Context, io_iterator_t Iterator)
{
    if (!Context)
        return;

    io_service_t device;

    while ((device = IOIteratorNext(Iterator)))
    {
        TorcUSBDevice usbdevice = GetDevice(device);
        TorcUSBPrivOSX* context = static_cast<TorcUSBPrivOSX*>(Context);

        if (!usbdevice.m_product.isEmpty() && context)
        {
            io_object_t notification;
            if (kIOReturnSuccess == IOServiceAddInterestNotification(
                        context->GetNotificationPort(), device,
                        kIOGeneralInterest,
                        (IOServiceInterestCallback)DeviceRemovedCallback,
                        context, &notification))
            {
                context->AddDevice(usbdevice, device, notification);
            }
        }

        IOObjectRelease(device);
    }
}

void TorcUSBPrivOSX::DeviceRemovedCallback(void *Context, io_service_t Service, natural_t Type, void *Argument)
{
    TorcUSBPrivOSX* context = static_cast<TorcUSBPrivOSX*>(Context);
    if (context && Type == kIOMessageServiceIsTerminated)
        context->RemoveDevice(Service);
}

void TorcUSBPrivOSX::RemoveDevice(io_service_t Service)
{
    bool found = false;

    {
        QMutexLocker locker(m_notificationsLock);
        found = m_notifications.contains(Service);
    }

    if (found)
    {
        TorcUSBDevice device;

        {
            QMutexLocker locker(m_notificationsLock);
            QPair<io_object_t,QString> pair = m_notifications.value(Service);
            device.m_path = pair.second;
            IOObjectRelease(pair.first);
            m_notifications.remove(Service);
        }

        ((TorcUSB*)parent())->DeviceRemoved(device);
        return;
    }

    LOG(VB_GENERAL, LOG_ERR, "Unknown device removed");
}

class USBFactoryOSX : public USBFactory
{
    void Score(int &Score)
    {
        if (Score <= 10)
            Score = 10;
    }

    TorcUSBPriv* Create(int Score, TorcUSB *Parent)
    {
        (void)Parent;

        if (Score <= 10)
            return new TorcUSBPrivOSX(Parent);

        return NULL;
    }
} USBFactoryOSX;
