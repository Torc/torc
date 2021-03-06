#ifndef TORCUSBPRIVUNIX_H
#define TORCUSBPRIVUNIX_H

// libudev
extern "C" {
#include <libudev.h>
}

// Qt
#include <QSocketNotifier>
#include <QObject>

// Torc
#include "torcusb.h"

class TorcUSBPrivUnix : public QObject, public TorcUSBPriv
{
    Q_OBJECT

  public:
    TorcUSBPrivUnix(TorcUSB *Parent);
    virtual ~TorcUSBPrivUnix();
    void Destroy         (void);
    void Refresh         (void);

  public slots:
    void SocketReadyRead (int Device);

  private:
    static enum TorcUSBDevice::Classes ToTorcClass(int UdevClass);

  private:
    TorcUSBDevice GetDevice    (udev_device *Device, bool Remove = false);
    void          AddDevice    (udev_device *Device);
    void          RemoveDevice (udev_device *Device);
    void          ChangeDevice (udev_device *Device);
    void          MoveDevice   (udev_device *Device);

  private:
    struct udev         *m_udev;
    struct udev_monitor *m_udevMonitor;
    int                  m_udevFD;
    QSocketNotifier     *m_socketNotifier;
};

#endif // TORCUSBPRIVUNIX_H
