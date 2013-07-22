/* Class TorcQMLFrameRate
*
* Copyright (C) Mark Kendall 2013
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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

// Torc
#include "torclogging.h"
#include "torcqmlframerate.h"

TorcQMLFrameRate::TorcQMLFrameRate()
  : m_timer(0),
    m_count(0),
    framesPerSecond(0.0)
{
    m_timer = startTimer(1000);
}

TorcQMLFrameRate::~TorcQMLFrameRate()
{
    if (m_timer)
        killTimer(m_timer);
}

qreal TorcQMLFrameRate::GetFramesPerSecond(void)
{
    return framesPerSecond;
}

void TorcQMLFrameRate::NewFrame(void)
{
    m_count++;
}

void TorcQMLFrameRate::timerEvent(QTimerEvent *Event)
{
    if (Event->timerId() == m_timer)
    {
        qreal newfps = (qreal)m_count;
        m_count = 0;
        LOG(VB_GUI, LOG_DEBUG, QString("FPS %1").arg(framesPerSecond));

        if (!qFuzzyCompare(newfps + 1.0, framesPerSecond + 1.0))
        {
            framesPerSecond = newfps;
            emit framesPerSecondChanged();
        }
    }
}
