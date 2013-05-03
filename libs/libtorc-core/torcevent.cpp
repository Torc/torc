/* Class TorcEvent
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
#include "torcevent.h"

/*! \class TorcEvent
 *  \brief A general purpose event object
 *
 *  TorcEvent is used to send Torc specific events to other objects.
 *  For a simple event message, the constructor requires only a Torc event
 *  type. For more complicated events, add addtional data with a QVariantMap.
 *
 * \sa TorcLocalContext::AddObserver
 * \sa TorcLocalContext::RemoveObserver
 * \sa TorcLocalContext::NotifyEvent
 */

QEvent::Type TorcEvent::TorcEventType = (QEvent::Type) QEvent::registerEventType();

TorcEvent::TorcEvent(int Event, const QVariantMap Data)
  : QEvent(TorcEventType),
    m_event(Event),
    m_data(Data)
{
}

TorcEvent::~TorcEvent()
{
}

int TorcEvent::Event(void)
{
    return m_event;
}

QVariantMap& TorcEvent::Data(void)
{
    return m_data;
}

TorcEvent* TorcEvent::Copy(void) const
{
    return new TorcEvent(m_event, m_data);
}
