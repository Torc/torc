#ifndef TORCUSB_H
#define TORCUSB_H

// Qt
#include <QVariant>
#include <QObject>
#include <QMutex>
#include <QStringList>

// Torc
#include "torccoreexport.h"
#include "http/torchttpservice.h"

class TORC_CORE_PUBLIC TorcUSBDevice
{
  public:
    enum Classes
    {
        Unknown      = -1,
        PerInterface = 0x00,
        Audio        = 0x01,
        Comm         = 0x02,
        HID          = 0x03,
        Physical     = 0x05,
        Still        = 0x06,
        Printer      = 0x07,
        MassStorage  = 0x08,
        Hub          = 0x09,
        Data         = 0x0a,
        AppSpec      = 0xfe,
        VendorSpec   = 0xff
    };

  public:
    TorcUSBDevice();
    TorcUSBDevice(const QString &Path, int VendorID, int ProductID, Classes Class);

    QVariantMap    ToMap         (void);
    static QString ClassToString (Classes Class);
    static bool    IgnoreClass   (Classes Class);

  public:
    QString m_path;
    int     m_vendorID;
    int     m_productID;
    Classes m_class;
    QString m_vendor;
    QString m_product;
};

class TORC_CORE_PUBLIC TorcUSBDeviceHandler
{
  public:
    static TorcUSBDeviceHandler *gUSBDeviceHandler;
    static int                   gDevicesSeen;

  public:
    enum KnownDevices
    {
        Unknown      = 0x0000,
        Nyxboard     = 0x0001
    };

    TorcUSBDeviceHandler();
    virtual ~TorcUSBDeviceHandler();

    static  bool DeviceHandled (const TorcUSBDevice &Device, bool Added);
    static  bool DeviceSeen    (KnownDevices Device);

  protected:
    virtual bool DeviceAdded   (const TorcUSBDevice &Device) = 0;
    virtual bool DeviceRemoved (const TorcUSBDevice &Device) = 0;

  private:
    TorcUSBDeviceHandler* GetNextHandler (void);

  private:
    TorcUSBDeviceHandler *m_nextUSBDeviceHandler;
};

class TorcUSBPriv;

class TORC_CORE_PUBLIC TorcUSB : public TorcHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",    "1.0.0")
    Q_CLASSINFO("GetDevices", "type=devices")

  public:
    TorcUSB();
    virtual ~TorcUSB();

  public slots:
    QVariantMap  GetDevices     (void);

  public:
    void         DeviceAdded    (const TorcUSBDevice &Device);
    void         DeviceRemoved  (const TorcUSBDevice &Device);

  protected:
    bool         event          (QEvent *Event);

  private:
    TorcUSBPriv *m_priv;
    QMap<QString,TorcUSBDevice> m_managedDevices;
    QMutex      *m_managedDevicesLock;
};

class TORC_CORE_PUBLIC TorcUSBPriv
{
  public:
    static TorcUSBPriv* Create  (TorcUSB *Parent);
    virtual void        Destroy (void) = 0;
    virtual void        Refresh (void) = 0;
};

class TORC_CORE_PUBLIC USBFactory
{
  public:
    USBFactory();
    virtual ~USBFactory();

    static USBFactory*   GetUSBFactory   (void);
    USBFactory*          NextFactory     (void) const;
    virtual void         Score           (int &Score) = 0;
    virtual TorcUSBPriv* Create          (int Score, TorcUSB *Parent) = 0;

  protected:
    static USBFactory*   gUSBFactory;
    USBFactory*          nextUSBFactory;
};


#endif // TORCUSB_H
