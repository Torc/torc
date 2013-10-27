/* Class TorcAdminThread
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
#include <QMutex>

// Torc
#include "torclocalcontext.h"
#include "torclogging.h"
#include "torcadminthread.h"

/*! \class TorcAdminThread
 *  \brief A simple thread to launch helper objects outside of the main loop.
 *
 *  When run, TorcAdminThread will iterate over each statically registered TorcAdminObject
 *  subclass and call create on each. When the thread is closing, objects are then destroyed
 *  in reverse order.
 *
 * \sa TorcAdminObject
 *
 * \todo Add a mechanism to add and remove TorcAdminObject's dynamically
*/

TorcAdminThread::TorcAdminThread()
  : TorcQThread(TORC_ADMIN_THREAD)
{
}

TorcAdminThread::~TorcAdminThread()
{
}

void TorcAdminThread::Start(void)
{
    LOG(VB_GENERAL, LOG_INFO, "Admin thread starting");

    // create objects that will run in the admin thread
    TorcAdminObject::CreateObjects();
}

void TorcAdminThread::Finish(void)
{
    TorcAdminObject::DestroyObjects();
    LOG(VB_GENERAL, LOG_INFO, "Admin thread stopping");
}

/*! \class TorcAdminObject
 *  \brief A factory class for automatically running objects outside of the main loop.
 *
 *  TorcLocalContext creates a helper thread, TorcAdminThread, when the application
 *  is started. TorcAdminThread then calls Create on each static subclass of TorcAdminObject
 *  to automatically run certain core functionality outside of the main loop.
 *
 *  Items are created in priority order, with higher priority objects created first.
 *
 * \sa TorcAdminThread
 *
 * \note There is currently no mechanism to dynamically add TorcAdminObjects at run time (e.g. plugins).
 *
 * Example usage:
 *
 * \code
 * static class MyBackgroundActivity : public TorcAdminObject
 * {
 *   public:
 *     MyBackgroundActivity()
 *       : TorcAdminObject(TORC_ADMIN_HIGH_PRIORITY),
 *         MyWorldDominationObject(NULL)
 *     {
 *     }
 *
 *     void Create(void)
 *     {
 *         MyWorldDominationObject = new Dominator();
       }
 *
 *     void Destroy(void)
 *     {
 *         delete MyWorldDominationObject;
 *     }
 * } MyBackgroundActivity;
 * \endcode
*/

QList<TorcAdminObject*> TorcAdminObject::gTorcAdminObjects;
TorcAdminObject* TorcAdminObject::gTorcAdminObject = NULL;
QMutex* TorcAdminObject::gTorcAdminObjectsLock = new QMutex(QMutex::Recursive);

TorcAdminObject::TorcAdminObject(int Priority)
  : m_priority(Priority)
{
    QMutexLocker lock(gTorcAdminObjectsLock);
    m_nextTorcAdminObject = gTorcAdminObject;
    gTorcAdminObject = this;
}

TorcAdminObject::~TorcAdminObject()
{
}

int TorcAdminObject::Priority(void) const
{
    return m_priority;
}

TorcAdminObject* TorcAdminObject::GetNextObject(void)
{
    return m_nextTorcAdminObject;
}

/*! \fn    TorcAdminObject::HigherPriority
 *  \brief Sort TorcAdminObject's based on relative priorities
*/
bool TorcAdminObject::HigherPriority(const TorcAdminObject *Object1, const TorcAdminObject *Object2)
{
    return Object1->Priority() > Object2->Priority();
}

/*! \fn    TorcAdminObject::CreateObjects
 *  \brief Iterates through the list of registered TorcAdminObject's and creates them
*/
void TorcAdminObject::CreateObjects(void)
{
    QMutexLocker lock(gTorcAdminObjectsLock);

    // only call this once
    static bool objectscreated = false;
    if (objectscreated)
        return;
    objectscreated = true;

    // create a list of static objects that can be sorted by priority.
    // Dynamically created objects will be appended to this list.
    TorcAdminObject* object = gTorcAdminObject;
    while (object)
    {
        gTorcAdminObjects.append(object);
        object = object->GetNextObject();
    }

    if (gTorcAdminObjects.isEmpty())
        return;

    qSort(gTorcAdminObjects.begin(), gTorcAdminObjects.end(), TorcAdminObject::HigherPriority);

    QList<TorcAdminObject*>::iterator it = gTorcAdminObjects.begin();
    for ( ; it != gTorcAdminObjects.end(); ++it)
        (*it)->Create();
}

/*! \fn    TorcAdminObject::DestroyObjects
 *  \brief Destroys each created admin object
*/
void TorcAdminObject::DestroyObjects(void)
{
    QMutexLocker lock(gTorcAdminObjectsLock);

    // destroy in the reverse order of creation
    QList<TorcAdminObject*>::iterator it = gTorcAdminObjects.end();
    while (it != gTorcAdminObjects.begin())
    {
        --it;
        (*it)->Destroy();
    }
}

