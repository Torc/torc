/* Class TorcObservable
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
#include <QCoreApplication>
#include <QMutex>

// Torc
#include "torcobservable.h"

/*! \class TorcObservable
 *  \brief TorcObservable will send event notifcations to registered 'listeners'.
 *
 * Classes that inherit from TorcObserval can notify registered listeners of important
 * events via the Notify method.
 *
 * This should only be used by classes that expect a more complex interaction with dependent objects. For
 * simple use cases (i.e. a small number of events and/or a small number of listeners), direct eventing or
 * the signal/slot mechanism should be used.
*/
TorcObservable::TorcObservable()
  : m_observerLock(new QMutex(QMutex::Recursive))
{
}

TorcObservable::~TorcObservable()
{
    delete m_observerLock;
}

///brief Register the given object to receive events.
void TorcObservable::AddObserver(QObject *Observer)
{
    QMutexLocker locker(m_observerLock);
    if (!Observer || m_observers.contains(Observer))
        return;
    m_observers.append(Observer);
}

///brief Deregister the given object.
void TorcObservable::RemoveObserver(QObject *Observer)
{
    QMutexLocker locker(m_observerLock);
    while (m_observers.contains(Observer))
        m_observers.removeOne(Observer);
}

///Brief Send the given event to each registered listener/observer.
void TorcObservable::Notify(const TorcEvent &Event)
{
    QMutexLocker locker(m_observerLock);

    foreach (QObject* observer, m_observers)
        QCoreApplication::postEvent(observer, Event.Copy());
}
