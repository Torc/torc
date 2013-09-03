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
#include <QStringList>

// Std
#include <time.h>
#include <errno.h>

// Torc
#include "torcconfig.h"
#include "torccompat.h"
#include "torccoreutils.h"

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
