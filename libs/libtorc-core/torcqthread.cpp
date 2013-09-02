/* Class TorcQThread
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
#include "torclocalcontext.h"
#include "torcloggingimp.cpp"
#include "torcqthread.h"

/*! \class TorcQThread
 *  \brief A Torc specific wrapper around QThread.
 *
 * TorcQThread performs basic Torc thread setup when a new thread is started (principally
 * logging initialisation and random number seeding) and cleanup (logging and database cleanup)
 * when a thread is exiting.
 *
 * Subclass TorcQThread and reimplement Start and Finish to add additional code that will
 * be executed in the new thread after initialisation and just before exiting respectively.
 *
 * To trigger code in other objects, use the TorcQThread specific Started and Finished signals
 * (instead of 'started' and 'finished', as provided by QThread).
 *
 * \note Torc database access is restricted to full TorcQThreads and is disallowed from QRunnable subclasses.
 *
 * \todo Make run private again and enfore event driven loops for all classes.
*/

static QThread *gMainThread  = NULL;
static QThread *gAdminThread = NULL;

void TorcQThread::SetMainThread(void)
{
    gMainThread = QThread::currentThread();
}

void TorcQThread::SetAdminThread(void)
{
    gAdminThread = QThread::currentThread();
}

bool TorcQThread::IsMainThread(void)
{
    if (gMainThread)
        return QThread::currentThread() == gMainThread;
    return QThread::currentThread()->objectName() == TORC_MAIN_THREAD;
}

bool TorcQThread::IsAdminThread(void)
{
    if (gAdminThread)
        return QThread::currentThread() == gAdminThread;
    return QThread::currentThread()->objectName() == TORC_ADMIN_THREAD;
}

QThread* TorcQThread::GetAdminThread(void)
{
    return gAdminThread;
}

TorcQThread::TorcQThread(const QString &Name)
  : QThread()
{
    // this ensures database access is allowed if needed
    setObjectName(Name);
}

TorcQThread::~TorcQThread()
{
}

void TorcQThread::run(void)
{
    Initialise();
    QThread::run();
    Deinitialise();
}

///\brief Performs Torc specific thread initialisation.
void TorcQThread::Initialise(void)
{
    RegisterLoggingThread(objectName());
    qsrand(QDateTime::currentDateTime().toTime_t() ^ QTime::currentTime().msec());

    Start();

    emit Started();
}

///\brief Performs Torc specific thread cleanup.
void TorcQThread::Deinitialise(void)
{
    Finish();

    emit Finished();

    // ensure database connections are released
    gLocalContext->CloseDatabaseConnections();
    DeregisterLoggingThread();
}
