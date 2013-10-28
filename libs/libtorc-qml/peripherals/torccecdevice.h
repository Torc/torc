#ifndef TORCCECDEVICE_H
#define TORCCECDEVICE_H

// Qt
#include <QObject>

// Torc
#include "torcqthread.h"

#define CEC_KEYPRESS_CONTEXT QString("CEC")

// settings
#define LIBCEC_ENABLED            (TORC_GUI + QString("CECEnabled"))
#define LIBCEC_DEVICE             (TORC_GUI + QString("CECDevice"))          // e.g. /dev/ttyACM0
#define LIBCEC_PHYSICALADDRESS    (TORC_GUI + QString("CECPhysicalAddress")) // e.g. 0x1000
#define LIBCEC_HDMIPORT           (TORC_GUI + QString("CECHDMIPort"))        // e.g. 1 - 4
#define LIBCEC_BASEDEVICE         (TORC_GUI + QString("CECBaseDevice"))      // e.g. CEC_DEVICE_TYPE_TV

#define LIBCEC_POWERONTV_ONSTART  (TORC_GUI + QString("CECPowerOnTVOnStart"))
#define LIBCEC_POWEROFFTV_ONEXIT  (TORC_GUI + QString("CECPowerOffTVOnExit"))
#define LIBCEC_MAKEACTIVESOURCE   (TORC_GUI + QString("CECMakeActiveSource"))
#define LIBCEC_SENDINACTIVESOURCE (TORC_GUI + QString("CECSendInactiveSource"))

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

class TorcCECDevicePriv;

class TorcCECDevice : public QObject
{
    friend class TorcCECThread;

    Q_OBJECT

  protected:
    TorcCECDevice();
    virtual ~TorcCECDevice();

    bool                event         (QEvent *Event);
    void                Open          (void);
    void                Close         (void);

  private:
    TorcCECDevicePriv  *m_priv;
    int                 m_retryCount;
    int                 m_retryTimer;
};

class TorcCECThread : public TorcQThread
{
    Q_OBJECT

  public:
    TorcCECThread();
    ~TorcCECThread();

    void               Start          (void);
    void               Finish         (void);

  private:
    TorcCECDevice     *m_device;
};

#endif // TORCCECDEVICE_H
