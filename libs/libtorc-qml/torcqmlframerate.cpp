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

#define MAX_FRAME_COUNT 60

TorcQMLFrameRate::TorcQMLFrameRate()
  : m_count(0),
    framesPerSecond(0.0)
{
    m_timer.start();
    m_timeout = new QTimer(this);
    connect(m_timeout, SIGNAL(timeout()), this, SLOT(Timeout()));
    m_timeout->setTimerType(Qt::VeryCoarseTimer);
    m_timeout->start(5000);
}

TorcQMLFrameRate::~TorcQMLFrameRate()
{
    delete m_timeout;
}

qreal TorcQMLFrameRate::GetFramesPerSecond(void)
{
    return framesPerSecond;
}

void TorcQMLFrameRate::NewFrame(void)
{
    if (++m_count == MAX_FRAME_COUNT)
        Update();
}

void TorcQMLFrameRate::Timeout(void)
{
    if (m_timer.elapsed() > 2000)
        Update();
}

void TorcQMLFrameRate::Update(void)
{
    qreal newfps = (m_count * 1000.0f) / m_timer.restart();
    m_count = 0;

    if (!qFuzzyCompare(newfps + 1.0, framesPerSecond + 1.0))
    {
        framesPerSecond = newfps;
        emit framesPerSecondChanged();
    }
}
