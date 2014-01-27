#ifndef TORCDIRECTORIES_H
#define TORCDIRECTORIES_H

// Qt
#include <QString>

// Torc
#include "torccoreexport.h"

TORC_CORE_PUBLIC void      InitialiseTorcDirectories (void);
TORC_CORE_PUBLIC QString   GetTorcConfigDir          (void);
TORC_CORE_PUBLIC QString   GetTorcShareDir           (void);
TORC_CORE_PUBLIC QString   GetTorcPluginDir          (void);
TORC_CORE_PUBLIC QString   GetTorcTransDir           (void);

#endif // TORCDIRECTORIES_H
