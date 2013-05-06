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

// Torc
#include "torclogging.h"
#include "torccocoa.h"
#include "torcadminthread.h"
#include "torcrunlooposx.h"

CFRunLoopRef gAdminRunLoop = 0;

/*! \brief A simple thread class encapsulating a CFRunLoop
 *
 *  A CFRunLoop is only run by default in the main thread and a QThread
 *  generally cannot be created to run both a QEventLoop and CFRunLoop.
 *  TorcRunLoopOSX is a singleton thread that is created by TorcAdminThread for
 *  the purpose of receiving callbacks from OS X frameworks such as IOKit and
 *  DiskArbitration.
 *
 * \sa gAdminRunLoop
 *
 *  \note This may be better implemented by combining QEventLoop::processEvents(QEventLoop::AllEvents, 10ms)
 *  and CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, false) in TorcAdminThread.
*/

static class TorcRunLoopOSX : public TorcThread, public TorcAdminObject
{
  public:
    TorcRunLoopOSX()
      : TorcThread("OSXRunLoop"),
        TorcAdminObject(TORC_ADMIN_CRIT_PRIORITY),
        m_stop(false)
    {
    }

    ~TorcRunLoopOSX()
    {
        Destroy();
    }

    void Stop(void)
    {
        if (gAdminRunLoop)
            CFRunLoopStop(gAdminRunLoop);
        m_stop = true;
        quit();
    }

    void Create(void)
    {
        start();

        int count = 0;
        while (count++ < 10 && !gAdminRunLoop)
            TorcThread::msleep(count < 2 ? 10 : 100);

        if (!gAdminRunLoop)
            LOG(VB_GENERAL, LOG_WARNING, "OS X callback thread not started yet!");
    }

    void Destroy(void)
    {
        Stop();
        wait();
    }

  protected:
    void run(void)
    {
        RunProlog();
        LOG(VB_GENERAL, LOG_INFO, "OSX callback thread starting");

        gAdminRunLoop = CFRunLoopGetCurrent();

        // run the loop
        while (!m_stop)
        {
            TorcThread::msleep(20);
            CFRunLoopRun();
        }

        gAdminRunLoop = 0;

        LOG(VB_GENERAL, LOG_INFO, "OSX callback thread stopping");
        RunEpilog();
    }

  private:
    bool            m_stop;

} TorcRunLoopOSX;

