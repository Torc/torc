#ifndef TORCOSXUTILS_H
#define TORCOSXUTILS_H

// OS X
#include <CoreFoundation/CoreFoundation.h>

// Qt
#include <QString>

// Torc
#include "torccoreexport.h"

QString TORC_CORE_PUBLIC CFStringReftoQString(CFStringRef String);

#endif // TORCOSXUTILS_H
