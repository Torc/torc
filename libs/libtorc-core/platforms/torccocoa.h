#ifndef TORCCOCOA_H
#define TORCCOCOA_H

// Std
#import <ApplicationServices/ApplicationServices.h>

// Torc
#include "torccoreexport.h"

TORC_CORE_PUBLIC CGDirectDisplayID GetOSXCocoaDisplay(void* Window);

class TORC_CORE_PUBLIC CocoaAutoReleasePool
{
  public:
    CocoaAutoReleasePool();
   ~CocoaAutoReleasePool();

  private:
    void *m_pool;
};

#endif // TORCCOCOA_H
