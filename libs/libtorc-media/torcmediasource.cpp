/* Class TorcMediaSource
*
* This file is part of the Torc project.
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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Torc
#include "torclogging.h"
#include "torcthread.h"
#include "torcadminthread.h"
#include "torcmediamaster.h"
#include "torcmediasource.h"

#define TORC_MEDIA_THREAD QString("MediaLoop")

/*! \class TorcMediaThread
 *  \brief A simple thread to run media scanners.
 *
 *  When run, TorcMediaThread will iterate over each statically registered TorcMediaSource
 *  subclass and call Create on each. Sources are then destoyed when the thread is closed.
 *
 * \sa TorcMediaSource
 * \sa TorcMediaMaster
 * \sa TorcMediaThreadObject
*/

class TorcMediaThread : public TorcThread
{
  public:
    TorcMediaThread()
      : TorcThread(TORC_MEDIA_THREAD)
    {
    }

    virtual ~TorcMediaThread()
    {
    }

    void run(void)
    {
        RunProlog();
        LOG(VB_GENERAL, LOG_INFO, "Media thread starting");

        // create objects that will run in the admin thread
        TorcMediaSource::CreateSources();

        // run the event loop
        exec();

        // destroy admin objects
        TorcMediaSource::DestroySources();

        LOG(VB_GENERAL, LOG_INFO, "Media thread stopping");
        RunEpilog();
    }
};

/*! \class TorcMediaThreadObject
 *  \brief A static class to create and run the TorcMediaThread object.
 *
 * \sa TorcMediaSource
 * \sa TorcMediaThread
*/

class TorcMediaThreadObject : public TorcAdminObject
{
  public:
    TorcMediaThreadObject()
      : TorcAdminObject(TORC_ADMIN_LOW_PRIORITY),
        m_mediaThread(NULL)
    {
    }

    void Create(void)
    {
        Destroy();

        // create the master media source in the admin thread first
        gTorcMediaMaster = new TorcMediaMaster();

        m_mediaThread = new TorcMediaThread();
        m_mediaThread->start();
    }

    void Destroy(void)
    {
        if (m_mediaThread)
        {
            m_mediaThread->quit();
            m_mediaThread->wait();
            delete m_mediaThread;
            m_mediaThread = NULL;
        }

        if (gTorcMediaMaster)
            gTorcMediaMaster->deleteLater();
        gTorcMediaMaster = NULL;
    }

  private:
    TorcMediaThread *m_mediaThread;

} TorcMediaThreadObject;

/*! \class TorcMediaSource
 *  \brief The base class for sources of media content.
 *
 *  Subclass TorcMediaSource to make media from a given source available
 *  to Torc.
 *
 * \sa TorcMediaThread
 * \sa TorcMediaThreadObject
*/

TorcMediaSource* TorcMediaSource::gTorcMediaSource = NULL;

TorcMediaSource::TorcMediaSource(void)
{
    m_nextTorcMediaSource = gTorcMediaSource;
    gTorcMediaSource = this;
}

TorcMediaSource::~TorcMediaSource()
{
}

TorcMediaSource* TorcMediaSource::GetNextSource(void)
{
    return m_nextTorcMediaSource;
}

void TorcMediaSource::CreateSources(void)
{
    TorcMediaSource* source = gTorcMediaSource;
    for ( ; source; source = source->GetNextSource())
        source->Create();
}

void TorcMediaSource::DestroySources(void)
{
    TorcMediaSource* source = gTorcMediaSource;
    for ( ; source; source = source->GetNextSource())
        source->Destroy();
}

