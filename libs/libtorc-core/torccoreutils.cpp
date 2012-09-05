/* TorcCoreUtils
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012
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

// Std
#include <time.h>

// Torc
#include "torccompat.h"
#include "torccoreutils.h"

QDateTime TorcDateTimeFromString(const QString &String)
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

quint64 GetMicrosecondCount(void)
{
#ifdef _MSC_VER
    LARGE_INTEGER ticksPerSecond;
    LARGE_INTEGER ticks;
    if (QueryPerformanceFrequency(&ticksPerSecond))
    {
        QueryPerformanceCounter(&ticks);
        return (quint64)(((qreal)ticks.QuadPart / ticksPerSecond.QuadPart) * 1000000);
    }
    return GetTickCount() * 1000;
#elif __APPLE__
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
