#ifndef TORCRUNLOOPOSX_H
#define TORCRUNLOOPOSX_H

// OS X
#include <CoreFoundation/CoreFoundation.h>

// Torc
#include "torccoreexport.h"

/*! \brief A reference to the global administration CFRunLoop.
 *
 *  This is used by various OS X objects that require a CFRunLoop (other
 *  than the main loop) for callback purposes.
 */
extern TORC_CORE_PUBLIC CFRunLoopRef gAdminRunLoop;

#endif // TORCRUNLOOPOSX_H
