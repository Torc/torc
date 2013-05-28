/* Class TorcReferenceCounted
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
#include <QObject>

// Torc
#include "torcreferencecounted.h"

/*! \class TorcReferenceCounter
 *  \brief A reference counting implementation.
 *
 * \todo Review use of EventLoopEnding which may not be a good idea outside of the main thread.
*/

bool TorcReferenceCounter::m_eventLoopEnding = false;

void TorcReferenceCounter::EventLoopEnding(bool Ending)
{
    m_eventLoopEnding = Ending;
}

TorcReferenceCounter::TorcReferenceCounter(void)
{
    UpRef();
}

TorcReferenceCounter::~TorcReferenceCounter()
{
}

void TorcReferenceCounter::UpRef(void)
{
    m_refCount.ref();
}

bool TorcReferenceCounter::DownRef(void)
{
    if (!m_refCount.deref())
    {
        QObject* object = dynamic_cast<QObject*>(this);
        if (object && !m_eventLoopEnding)
        {
            object->deleteLater();
            return true;
        }

        delete this;
        return true;
    }

    return false;
}

bool TorcReferenceCounter::IsShared(void)
{
    return m_refCount.fetchAndAddOrdered(0) > 1;
}

/*! \class TorcReferenceLocker
 *  \brief A convenience class to maintain a reference to an object within a given scope.
*/

TorcReferenceLocker::TorcReferenceLocker(TorcReferenceCounter *Counter)
  : m_refCountedObject(Counter)
{
    if (m_refCountedObject)
        m_refCountedObject->UpRef();
}

TorcReferenceLocker::~TorcReferenceLocker(void)
{
    if (m_refCountedObject)
        m_refCountedObject->DownRef();
    m_refCountedObject = NULL;
}
