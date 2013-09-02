/* Class UITimer
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

// Torc
#include "torccoreutils.h"
#include "uitimer.h"

UITimer::UITimer(qreal Interval)
  : m_interval(Interval),
    m_nextEvent(0)
{
}

void UITimer::Start(void)
{
    m_nextEvent = TorcCoreUtils::GetMicrosecondCount() + m_interval;
}

void UITimer::Wait(void)
{
    qint64 remaining = m_nextEvent - TorcCoreUtils::GetMicrosecondCount();
    if (remaining > 0 && remaining < (m_interval * 4))
        TorcCoreUtils::USleep(remaining);
}

void UITimer::Reset(void)
{
    m_nextEvent = 0;
}

void UITimer::SetInterval(qreal Interval)
{
    m_interval = Interval;
}
