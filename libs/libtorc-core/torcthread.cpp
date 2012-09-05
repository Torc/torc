/* Class TorcThread
 * Based on MThread from MythTV, copyright Daniel Kristhansson (below)
 *
*/

/*  -*- Mode: c++ -*-
 *
 *   Class MThread
 *
 *   Copyright (C) Daniel Kristjansson 2011
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// Std
#include <iostream>
using namespace std;

// Qt
#include <QStringList>
#include <QTimerEvent>
#include <QRunnable>
#include <QtGlobal>
#include <QMutex>
#include <QSet>

// Torc
#include "torclocalcontext.h"
#include "torcdb.h"
#include "torctimer.h"
#include "torclogging.h"
#include "torcloggingimp.h"
#include "torcthread.h"

/* FIXME - no db here yet
class DBPurgeHandler : public QObject
{
  public:
    DBPurgeHandler()
    {
        purgeTimer = startTimer(5 * 60000);
    }
    void timerEvent(QTimerEvent *event)
    {
        if (event->timerId() == purgeTimer)
            GetMythDB()->GetDBManager()->PurgeIdleConnections(false);
    }
    int purgeTimer;
};
*/

class TorcThreadInternal : public QThread
{
  public:
    TorcThreadInternal(TorcThread &parent) : m_parent(parent) { }

    virtual void run(void)
    {
        m_parent.run();
    }

    void QThreadRun(void)
    {
        QThread::run();
    }

    int exec(void)
    {
        //DBPurgeHandler ph();
        return QThread::exec();
    }

    static void SetTerminationEnabled(bool enabled = true)
    {
        QThread::setTerminationEnabled(enabled);
    }

    static void Sleep(unsigned long time)
    {
        QThread::sleep(time);
    }

    static void MSleep(unsigned long time)
    {
        QThread::msleep(time);
    }

    static void USleep(unsigned long time)
    {
        QThread::usleep(time);
    }

  private:
    TorcThread &m_parent;
};

static QSet<TorcThread*> gTorcThreads;
static QMutex            gTorcThreadsLock;

TorcThread::TorcThread(const QString &ObjectName) :
    m_thread(new TorcThreadInternal(*this)),
    m_prologExecuted(true),
    m_epilogExecuted(true)
{
    m_thread->setObjectName(ObjectName);
    QMutexLocker locker(&gTorcThreadsLock);
    gTorcThreads.insert(this);
}

TorcThread::~TorcThread()
{
    if (!m_prologExecuted)
        LOG(VB_GENERAL, LOG_CRIT, "TorcThread prolog was never run.");

    if (!m_epilogExecuted)
        LOG(VB_GENERAL, LOG_CRIT, "TorcThread epilog was never run.");

    if (m_thread->isRunning())
    {
        LOG(VB_GENERAL, LOG_CRIT,
            "TorcThread destructor called while thread still running.");
        m_thread->wait();
    }

    {
        QMutexLocker locker(&gTorcThreadsLock);
        gTorcThreads.remove(this);
    }

    delete m_thread;
    m_thread = NULL;
}

bool TorcThread::IsMainThread(void)
{
    return QThread::currentThread()->objectName() == TORC_MAIN_THREAD;
}

bool TorcThread::IsCurrentThread(TorcThread *thread)
{
    if (!thread)
        return false;

    return QThread::currentThread() == thread->GetQThread();
}

bool TorcThread::IsCurrentThread(QThread *thread)
{
    if (!thread)
        return false;

    return QThread::currentThread() == thread;
}

void TorcThread::Cleanup(void)
{
    QMutexLocker locker(&gTorcThreadsLock);
    QSet<TorcThread*> orphans;
    QSet<TorcThread*>::const_iterator it;
    for (it = gTorcThreads.begin(); it != gTorcThreads.end(); ++it)
    {
        if ((*it)->isRunning())
        {
            orphans.insert(*it);
            (*it)->exit(1);
        }
    }

    if (orphans.empty())
        return;

    // logging has been stopped so we need to use iostream...
    cerr << "Error: Not all threads were shut down properly: " << endl;
    for (it = orphans.begin(); it != orphans.end(); ++it)
    {
        cerr << "Thread " << qPrintable((*it)->objectName())
             << " is still running" << endl;
    }
    cerr << endl;

    static const int kTimeout = 5000;
    TorcTimer t;
    t.Start();
    for (it = orphans.begin();
         it != orphans.end() && t.Elapsed() < kTimeout; ++it)
    {
        int left = kTimeout - t.Elapsed();
        if (left > 0)
            (*it)->wait(left);
    }
}

void TorcThread::GetAllThreadNames(QStringList &list)
{
    QMutexLocker locker(&gTorcThreadsLock);
    QSet<TorcThread*>::const_iterator it;
    for (it = gTorcThreads.begin(); it != gTorcThreads.end(); ++it)
        list.push_back((*it)->objectName());
}

void TorcThread::GetAllRunningThreadNames(QStringList &list)
{
    QMutexLocker locker(&gTorcThreadsLock);
    QSet<TorcThread*>::const_iterator it;
    for (it = gTorcThreads.begin(); it != gTorcThreads.end(); ++it)
    {
        if ((*it)->isRunning())
            list.push_back((*it)->objectName());
    }
}

void TorcThread::RunProlog(void)
{
    if (QThread::currentThread() != m_thread)
    {
        LOG(VB_GENERAL, LOG_CRIT,
            "RunProlog can only be executed in the run() method of a thread.");
        return;
    }
    setTerminationEnabled(false);
    ThreadSetup(m_thread->objectName());
    m_prologExecuted = true;
}

void TorcThread::RunEpilog(void)
{
    if (QThread::currentThread() != m_thread)
    {
        LOG(VB_GENERAL, LOG_CRIT,
            "RunEpilog can only be executed in the run() method of a thread.");
        return;
    }
    ThreadCleanup();
    m_epilogExecuted = true;
}

void TorcThread::ThreadSetup(const QString &Name)
{
    RegisterLoggingThread(Name);
    qsrand(QDateTime::currentDateTime().toTime_t() ^
           QTime::currentTime().msec());
}

void TorcThread::ThreadCleanup(void)
{
    DeregisterLoggingThread();
    if (gLocalContext)
        gLocalContext->CloseDatabaseConnections();
}

QThread *TorcThread::GetQThread(void)
{
    return m_thread;
}

void TorcThread::setObjectName(const QString &name)
{
    m_thread->setObjectName(name);
}

QString TorcThread::objectName(void) const
{
    return m_thread->objectName();
}

void TorcThread::setPriority(QThread::Priority priority)
{
    m_thread->setPriority(priority);
}

QThread::Priority TorcThread::priority(void) const
{
    return m_thread->priority();
}

bool TorcThread::isFinished(void) const
{
    return m_thread->isFinished();
}

bool TorcThread::isRunning(void) const
{
    return m_thread->isRunning();
}

void TorcThread::setStackSize(uint stackSize)
{
    m_thread->setStackSize(stackSize);
}

uint TorcThread::stackSize(void) const
{
    return m_thread->stackSize();
}

void TorcThread::exit(int retcode)
{
    m_thread->exit(retcode);
}

void TorcThread::start(QThread::Priority p)
{
    m_prologExecuted = false;
    m_epilogExecuted = false;
    m_thread->start(p);
}

void TorcThread::terminate(void)
{
    m_thread->terminate();
}

void TorcThread::quit(void)
{
    m_thread->quit();
}

bool TorcThread::wait(unsigned long time)
{
    if (m_thread->isRunning())
        return m_thread->wait(time);
    return true;
}

void TorcThread::run(void)
{
    RunProlog();
    m_thread->QThreadRun();
    RunEpilog();
}

int TorcThread::exec(void)
{
    return m_thread->exec();
}

void TorcThread::setTerminationEnabled(bool enabled)
{
    TorcThreadInternal::SetTerminationEnabled(enabled);
}

void TorcThread::sleep(unsigned long time)
{
    TorcThreadInternal::Sleep(time);
}

void TorcThread::msleep(unsigned long time)
{
    TorcThreadInternal::MSleep(time);
}

void TorcThread::usleep(unsigned long time)
{
    TorcThreadInternal::USleep(time);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
