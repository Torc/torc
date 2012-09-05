#ifndef TORCPOWEROSX_H
#define TORCPOWEROSX_H

// OS X
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>

// Torc
#include "torcpower.h"

class TorcPowerOSX : public TorcPowerPriv
{
    Q_OBJECT

  public:
    TorcPowerOSX(TorcPower *Parent);
    virtual ~TorcPowerOSX();

    bool Shutdown        (void);
    bool Suspend         (void);
    bool Hibernate       (void);
    bool Restart         (void);

  protected:
    static void PowerCallBack       (void *Reference, io_service_t Service,
                                     natural_t Type, void *Data);
    static void PowerSourceCallBack (void *Reference);
    void        GetPowerStatus      (void);

  private:
    CFRunLoopSourceRef    m_powerRef;
    io_connect_t          m_rootPowerDomain;
    io_object_t           m_powerNotifier;
    IONotificationPortRef m_powerNotifyPort;
};

#endif // TORCPOWEROSX_H
