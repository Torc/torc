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
 *  \brief A general purpose event object.
 *
 * TorcEvent is used to send Torc::Actions events to other objects.
 * For a simple event message, the constructor requires only a Torc event
 * type. For more complicated events, add additional data with a QVariantMap.
 *
 * To listen for Torc events, an object must be a QObject subclass and reimplement QObject::event.
 * It can then call gLocalContext->AddObserver(this) and remember
 * to call gLocalContext->RemoveObserver(this) when event notification is no longer
 * required.
 *
 * (N.B. Take care to return an appropriate value from event() and be wary of implementing other
 * QObject event handlers (such as timerEvent) as they can interact in unexpected ways).
 *
 * For specific functionality for which there are a known and/or limited number of 'listeners',
 * consider sending events directly with QCoreApplication::postEvent (or alternatively, use the
 * signal/slot mechanism, which is also thread safe).
 *
 * \sa TorcLocalContext::AddObserver
 * \sa TorcLocalContext::RemoveObserver
 * \sa TorcLocalContext::NotifyEvent
 *
 * \sa TorcLocalContext
 * \sa TorcObservable
 */

/// Register TorcEventType with QEvent
QEvent::Type TorcEvent::TorcEventType = (QEvent::Type) QEvent::registerEventType();

/// The default implementation contains no data.
TorcEvent::TorcEvent(int Event, const QVariantMap Data/* = QVariantMap()*/)
  : QEvent(TorcEventType),
    m_event(Event),
    m_data(Data)
{
}

TorcEvent::~TorcEvent()
{
}

/// \brief Return the Torc action associated with this event.
int TorcEvent::GetEvent(void)
{
    return m_event;
}

/// \brief Return a reference to the Data contained within this event.
QVariantMap& TorcEvent::Data(void)
{
    return m_data;
}

/*! \brief Copy this event.
 *
 * TorcObservable will iterate over the list of 'listening' objects and send events
 * to each using QCoreApplication::postEvent. postEvent will however take ownership of the
 * event object, hence we need to create a copy for each message.
*/
TorcEvent* TorcEvent::Copy(void) const
{
    return new TorcEvent(m_event, m_data);
}
