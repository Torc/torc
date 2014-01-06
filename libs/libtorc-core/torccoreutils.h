#ifndef TORCCOREUTILS_H
#define TORCCOREUTILS_H

// Qt
#include <QFile>
#include <QMetaEnum>
#include <QDateTime>

// Torc
#include "torccoreexport.h"

class TORC_CORE_PUBLIC TorcCoreUtils
{
  public:
    static QDateTime   DateTimeFromString    (const QString &String);
    static quint64     GetMicrosecondCount   (void);
    static QString     EnumsToScript         (const QMetaObject &MetaObject);
    static void        QtMessage             (QtMsgType Type, const QMessageLogContext &Context, const QString &Message);
    static bool        HasZlib               (void);
    static QByteArray* GZipCompress          (QByteArray *Source);
    static QByteArray* GZipCompressFile      (QFile *Source);
};

#endif // TORCCOREUTILS_H
