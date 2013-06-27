#ifndef TORCCOREUTILS_H
#define TORCCOREUTILS_H

// Qt
#include <QMetaEnum>
#include <QDateTime>

// Torc
#include "torccoreexport.h"

class TORC_CORE_PUBLIC TorcCoreUtils
{
  public:
    static QDateTime   DateTimeFromString    (const QString &String);
    static quint64     GetMicrosecondCount   (void);
    static void        USleep                (int USecs);
    static QString     EnumsToScript         (const QMetaObject &MetaObject);
};

#endif // TORCCOREUTILS_H
