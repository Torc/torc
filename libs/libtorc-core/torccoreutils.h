#ifndef TORCCOREUTILS_H
#define TORCCOREUTILS_H

// Qt
#include <QDateTime>

// Torc
#include "torccoreexport.h"

TORC_CORE_PUBLIC QDateTime   TorcDateTimeFromString    (const QString &String);
TORC_CORE_PUBLIC quint64     GetMicrosecondCount       (void);
TORC_CORE_PUBLIC void        TorcUSleep                (int USecs);

#endif // TORCCOREUTILS_H
