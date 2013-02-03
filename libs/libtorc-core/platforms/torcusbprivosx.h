#ifndef TORCUSBPRIVOSX_H
#define TORCUSBPRIVOSX_H

// OS X
#include <IOKit/usb/IOUSBLib.h>

// Qt
#include <QObject>
#include <QPair>
#include <QMap>

// Torc
#include "torcusb.h"

class TorcUSBPrivOSX : public QObject, public TorcUSBPriv
{
    Q_OBJECT

  public:
    TorcUSBPrivOSX(TorcUSB *Parent);
    virtual ~TorcUSBPrivOSX();
    void                  Destroy             (void);

    void                  AddDevice           (TorcUSBDevice &Device, io_service_t Service, io_object_t Notification);
    void                  RemoveDevice        (io_service_t Service);
    IONotificationPortRef GetNotificationPort (void);

  public:
    static void DeviceAddedCallback           (void *Context, io_iterator_t Iterator);
    static void DeviceRemovedCallback         (void *Context, io_service_t Service, natural_t Type, void *Argument);

  private:
    static TorcUSBDevice::Classes  ToTorcClass (int ClassType);
    static TorcUSBDevice           GetDevice   (io_service_t Device);

  private:
    CFRunLoopSourceRef             m_usbRef;
    IONotificationPortRef          m_usbNotifyPort;
    io_iterator_t                  m_iterator;
    QMutex                        *m_notificationsLock;
    QMap<io_service_t,QPair<io_object_t,QString> > m_notifications;
};

#endif // TORCUSBPRIVOSX_H
