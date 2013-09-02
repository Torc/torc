/* Class TorcRunLoopOSX
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
#include <QTimer>

// Torc
#include "torclogging.h"
#include "torccocoa.h"
#include "torcqthread.h"
#include "torcadminthread.h"
#include "torcrunlooposx.h"

CFRunLoopRef gAdminRunLoop = 0;
bool         gAdminRunLoopRunning = true;

/*! \class TorcOSXCallbackThread
 *  \brief TorcQThread subclass to run a CFRunLoop
 *
 *  A CFRunLoop is only run by default in the main thread and a QThread
 *  generally cannot be created to run both a QEventLoop and CFRunLoop.
 *  TorcOSXCallbackThread is a singleton thread that is created by TorcRunLoopOSX for
 *  the purpose of receiving callbacks from OS X frameworks such as IOKit and
 *  DiskArbitration.
 *
 * \sa gAdminRunLoop
 * \sa CallbackObject
 * \sa TorcRunLoopOSX
 *
 *  \note This may be better implemented by combining QEventLoop::processEvents(QEventLoop::AllEvents, 10ms)
 *  and CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, false) in TorcAdminThread.
*/

TorcOSXCallbackThread::TorcOSXCallbackThread()
  : TorcQThread("OSXRunLoop"),
    m_object(NULL)
{
}

TorcOSXCallbackThread::~TorcOSXCallbackThread()
{
    delete m_object;
}

void TorcOSXCallbackThread::Start(void)
{
    LOG(VB_GENERAL, LOG_INFO, "OSX callback thread starting");
    gAdminRunLoop = CFRunLoopGetCurrent();
    m_object = new CallbackObject();
}

void TorcOSXCallbackThread::Finish(void)
{
    delete m_object;
    m_object = NULL;
    gAdminRunLoop = 0;
    LOG(VB_GENERAL, LOG_INFO, "OSX callback thread stopping");
}

/*! \class CallbackObject
 *  \brief A simple class encapusulating a single timer to start the CFRunLoop
*/
CallbackObject::CallbackObject()
{
    // schedule the run loop to start
    if (gAdminRunLoopRunning)
        QTimer::singleShot(10, this, SLOT(Run()));
}

void CallbackObject::Run(void)
{
    // start the run loop
    if (gAdminRunLoopRunning)
        CFRunLoopRun();

    // if it exits, schedule it to start again. This usually means there is nothing
    // setup using a callback, so don't wast cycles and wait a while before restarting.
    if (gAdminRunLoopRunning)
        QTimer::singleShot(100, this, SLOT(Run()));
}

/*! \class TorcRunLoopOSX
 *  \brief A TorcAdminObject to instanciate the TorcOSXCallbackThread singleton.
*/
static class TorcRunLoopOSX : public TorcAdminObject
{
  public:
    TorcRunLoopOSX()
      : TorcAdminObject(TORC_ADMIN_CRIT_PRIORITY),
        m_thread(NULL)
    {
    }

    ~TorcRunLoopOSX()
    {
        Destroy();
    }

    void Create(void)
    {
        Destroy();

        gAdminRunLoopRunning = true;
        m_thread = new TorcOSXCallbackThread();
        m_thread->start();

        int count = 0;
        while (count++ < 10 && !m_thread->isRunning())
            QThread::msleep(count < 2 ? 10 : 100);

        if (!m_thread->isRunning())
            LOG(VB_GENERAL, LOG_WARNING, "OS X callback thread not started yet!");
    }

    void Destroy(void)
    {
        gAdminRunLoopRunning = false;

        if (gAdminRunLoop)
            CFRunLoopStop(gAdminRunLoop);

        if (m_thread)
        {
            m_thread->quit();
            m_thread->wait();
        }

        delete m_thread;
        m_thread = NULL;
    }

  private:
    TorcOSXCallbackThread *m_thread;
} TorcRunLoopOSX;

