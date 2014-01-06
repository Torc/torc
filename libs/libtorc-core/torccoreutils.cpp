/* TorcCoreUtils
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012-13
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Qt
#include <QThread>
#include <QStringList>

// Std
#include <time.h>
#include <errno.h>

// Torc
#include "torcconfig.h"
#include "torccompat.h"
#include "torclogging.h"
#include "torccoreutils.h"

// zlib
#if defined(CONFIG_ZLIB) && CONFIG_ZLIB
#include "zlib.h"
#endif

/// \brief Parse a QDataTime from the given QString
QDateTime TorcCoreUtils::DateTimeFromString(const QString &String)
{
    if (String.isEmpty())
        return QDateTime();

    if (!String.contains("-") && String.length() == 14)
    {
        // must be in yyyyMMddhhmmss format
        return QDateTime::fromString(String, "yyyyMMddhhmmss");
    }

    return QDateTime::fromString(String, Qt::ISODate);
}

/*! \brief Get the current system clock time in microseconds.
 *
 * If microsecond clock accuracy is not available, it is inferred from the best
 * resolution available.
*/
quint64 TorcCoreUtils::GetMicrosecondCount(void)
{
#ifdef Q_OS_WIN
    LARGE_INTEGER ticksPerSecond;
    LARGE_INTEGER ticks;
    if (QueryPerformanceFrequency(&ticksPerSecond))
    {
        QueryPerformanceCounter(&ticks);
        return (quint64)(((qreal)ticks.QuadPart / ticksPerSecond.QuadPart) * 1000000);
    }
    return GetTickCount() * 1000;
#elif defined(Q_OS_MAC)
    struct timeval timenow;
    gettimeofday(&timenow, NULL);
    return (timenow.tv_sec * 1000000) + timenow.tv_usec;
#else
    timespec timenow;
    clock_gettime(CLOCK_MONOTONIC, &timenow);
    return (timenow.tv_sec * 1000000) + ((timenow.tv_nsec + 500) / 1000);
#endif

    return 0;
}

/*! \brief Convert a QMetaEnum into a QString that can be passed to QScript.
 *
 * This function is used to pass the string representation of enumerations to a QScript
 * context. The enumerations can then be used directly within scripts.
 */
QString TorcCoreUtils::EnumsToScript(const QMetaObject &MetaObject)
{
    QString name   = MetaObject.className();
    QString result = QString("%1 = new Object();\n").arg(name);

    for (int i = 0; i < MetaObject.enumeratorCount(); ++i)
    {
        QMetaEnum metaenum = MetaObject.enumerator(i);

        for (int j = 0; j < metaenum.keyCount(); ++j)
            result += QString("%1.%2 = %3;\n").arg(name).arg(metaenum.key(j)).arg(metaenum.value(j));
    }

    return result;
}

/*! \brief A handler routine for Qt messages.
 *
 * This ensures Qt warnings are included in non-console logs.
 *
 * \todo Refactor logging to allow using Context directly in the log, hence removing line/function duplication.
*/
void TorcCoreUtils::QtMessage(QtMsgType Type, const QMessageLogContext &Context, const QString &Message)
{
    QString message = QString("(%1:%2) %3").arg(Context.file).arg(Context.line).arg(Message);

    switch (Type)
    {
        case QtFatalMsg:
            LOG(VB_GENERAL, LOG_CRIT, message);
            QThread::msleep(100);
            abort();
        case QtDebugMsg:
            LOG(VB_GENERAL, LOG_INFO, message);
            break;
        default:
            LOG(VB_GENERAL, LOG_ERR, message);
            break;
    }
}

///\brief Return true if zlib support is available.
bool TorcCoreUtils::HasZlib(void)
{
#if defined(CONFIG_ZLIB) && CONFIG_ZLIB
    return true;
#else
    return false;
#endif
}

/*! \brief Compress the supplied data using GZip.
 *
 * The returned data is suitable for sending as part of an HTTP response when the requestor accepts
 * gzip compression.
*/
QByteArray* TorcCoreUtils::GZipCompress(QByteArray *Source)
{
    QByteArray *result = NULL;

#if defined(CONFIG_ZLIB) && CONFIG_ZLIB
    // this shouldn't happen
    if (!Source || (Source && Source->size() < 0))
        return result;

    // initialise zlib (see zlib usage examples)
    static int chunksize = 16384;
    char outputbuffer[chunksize];

    z_stream stream;
    stream.zalloc   = NULL;
    stream.zfree    = NULL;
    stream.opaque   = NULL;
    stream.avail_in = Source->size();
    stream.next_in  = (Bytef*)Source->data();

    if (Z_OK != deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to setup zlip decompression");
        return result;
    }

    result = new QByteArray();

    do
    {
        stream.avail_out = chunksize;
        stream.next_out  = (Bytef*)outputbuffer;

        int error = deflate(&stream, Z_FINISH);

        if (Z_NEED_DICT == error || error < Z_OK)
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to compress data");
            deflateEnd(&stream);
            return result;
        }

        result->append(outputbuffer, chunksize - stream.avail_out);
    } while (stream.avail_out == 0);

    deflateEnd(&stream);
#endif

    return result;
}

/*! \brief Compress the given file using GZip.
 *
 * The returned data is suitable for sending as part of an HTTP response when the requestor accepts
 * gzip compression.
*/
QByteArray* TorcCoreUtils::GZipCompressFile(QFile *Source)
{
    QByteArray *result = NULL;

#if defined(CONFIG_ZLIB) && CONFIG_ZLIB
    // this shouldn't happen
    if (!Source || (Source && Source->size() < 0))
        return result;

    // initialise zlib (see zlib usage examples)
    static int chunksize = 32768;
    char inputbuffer[chunksize];
    char outputbuffer[chunksize];

    z_stream stream;
    stream.zalloc   = NULL;
    stream.zfree    = NULL;
    stream.opaque   = NULL;

    if (Z_OK != deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to setup zlip decompression");
        return result;
    }

    // this should have been checked already
    if (!Source->open(QIODevice::ReadOnly))
        return result;

    result = new QByteArray();

    while (Source->bytesAvailable() > 0)
    {
        stream.avail_out = chunksize;
        stream.next_out  = (Bytef*)outputbuffer;

        qint64 read = Source->read(inputbuffer, qMin(Source->bytesAvailable(), (qint64)chunksize));

        if (read > 0)
        {
            stream.avail_in = read;
            stream.next_in  = (Bytef*)inputbuffer;

            int error = deflate(&stream, Source->bytesAvailable() ? Z_SYNC_FLUSH : Z_FINISH);

            if (error == Z_OK || error == Z_STREAM_END)
            {
                result->append(outputbuffer, chunksize - stream.avail_out);
                continue;
            }
        }

        LOG(VB_GENERAL, LOG_ERR, "Failed to compress file");
        deflateEnd(&stream);
        return result;
    }

    deflateEnd(&stream);
#endif

    return result;
}
