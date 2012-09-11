#ifndef TORCCECDEVICE_H
#define TORCCECDEVICE_H

// Qt
#include <QObject>

// Torc
#include "torccoreexport.h"

#define CEC_KEYPRESS_CONTEXT QString("libCEC")

// settings
#define LIBCEC_ENABLED            QString("libCECEnabled")
#define LIBCEC_DEVICE             QString("libCECDevice")          // e.g. /dev/ttyACM0
#define LIBCEC_PHYSICALADDRESS    QString("libCECPhysicalAddress") // e.g. 0x1000
#define LIBCEC_HDMIPORT           QString("libCECHDMIPort")        // e.g. 1 - 4
#define LIBCEC_BASEDEVICE         QString("libCECBaseDevice")      // e.g. CEC_DEVICE_TYPE_TV

#define LIBCEC_POWERONTV_ONSTART  QString("libCECPowerOnTVOnStart")
#define LIBCEC_POWEROFFTV_ONEXIT  QString("libCECPowerOffTVOnExit")
#define LIBCEC_MAKEACTIVESOURCE   QString("libCECMakeActiveSource")
#define LIBCEC_SENDINACTIVESOURCE QString("libCECSendInactiveSource")

// setting defaults
#define LIBCEC_ENABLED_DEFAULT            true
#define LIBCEC_DEVICE_DEFAULT             QString("auto")
#define LIBCEC_PHYSICALADDRESS_DEFAULT    CEC_INVALID_PHYSICAL_ADDRESS
#define LIBCEC_HDMIPORT_DEFAULT           CEC_DEFAULT_HDMI_PORT
#define LIBCEC_BASEDEVICE_DEFAULT         CEC_DEFAULT_BASE_DEVICE

#define LIBCEC_POWEROFFTV_ONEXIT_DEFAULT  true
#define LIBCEC_POWERONTV_ONSTART_DEFAULT  true
#define LIBCEC_MAKEACTIVESOURCE_DEFAULT   true
#define LIBCEC_SENDINACTIVESOURCE_DEFAULT true

class QMutex;
class TorcCECDevicePriv;

class TorcCECDevice : public QObject
{
    Q_OBJECT

  public:
    static void Create  (void);
    static void Destroy (void);

  protected:
    TorcCECDevice();
    virtual ~TorcCECDevice();

    bool        event   (QEvent *Event);
    void        Open    (void);
    void        Close   (void);

  private:
    TorcCECDevicePriv *m_priv;
    int                m_retryCount;
    int                m_retryTimer;
};

extern TORC_CORE_PUBLIC TorcCECDevice *gCECDevice;
extern TORC_CORE_PUBLIC QMutex        *gCECDeviceLock;

#endif // TORCCECDEVICE_H
