#ifndef TORCPOWERUNIXDBUS_H
#define TORCPOWERUNIXDBUS_H

// Qt
#include <QtDBus>

// Torc
#include "torcpower.h"

class TorcPowerUnixDBus : public TorcPowerPriv
{
    Q_OBJECT

  public:
    static bool Available (void);

  public:
    TorcPowerUnixDBus(TorcPower *Parent);
    virtual ~TorcPowerUnixDBus();

    bool Shutdown         (void);
    bool Suspend          (void);
    bool Hibernate        (void);
    bool Restart          (void);

  public slots:
    void DeviceAdded      (QDBusObjectPath Device);
    void DeviceRemoved    (QDBusObjectPath Device);
    void DeviceChanged    (QDBusObjectPath Device);
    void DBusError        (QDBusError      Error);
    void DBusCallback     (void);
    void Changed          (void);

  private:
    void UpdateBattery    (void);
    int  GetBatteryLevel  (const QString &Path);
    void UpdateProperties (void);

  private:
    bool                  m_onBattery;
    QMap<QString,int>     m_devices;
    QMutex               *m_deviceLock;
    QDBusInterface       *m_upowerInterface;
    QDBusInterface       *m_consoleInterface;
};

#endif // TORCPOWERUNIXDBUS_H
