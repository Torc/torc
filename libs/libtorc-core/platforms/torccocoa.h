#ifndef TORCCOCOA_H
#define TORCCOCOA_H

// Std
#import <ApplicationServices/ApplicationServices.h>

// Qt
#include <QByteArray>

// Torc
#include "torccoreexport.h"

TORC_CORE_PUBLIC CGDirectDisplayID GetOSXCocoaDisplay (void* Window);
TORC_CORE_PUBLIC QByteArray        GetOSXEDID         (void);
TORC_CORE_PUBLIC QByteArray        GetOSXEDID         (CGDirectDisplayID Display);

class TORC_CORE_PUBLIC CocoaAutoReleasePool
{
  public:
    CocoaAutoReleasePool();
   ~CocoaAutoReleasePool();

  private:
    void *m_pool;
};

#endif // TORCCOCOA_H
