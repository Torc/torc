#ifndef TORCCOREUTILS_H
#define TORCCOREUTILS_H

// Qt
#include <QMetaEnum>
#include <QDateTime>

// Torc
#include "torccoreexport.h"

TORC_CORE_PUBLIC QDateTime   TorcDateTimeFromString    (const QString &String);
TORC_CORE_PUBLIC quint64     GetMicrosecondCount       (void);
TORC_CORE_PUBLIC void        TorcUSleep                (int USecs);
TORC_CORE_PUBLIC QString     EnumsToScript             (const QMetaObject &MetaObject);

#endif // TORCCOREUTILS_H
