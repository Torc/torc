/* Class AudioOutputListener/AudioOutputListeners
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012
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
#include "audiooutputlisteners.h"

/*! \class AudioOutputListener
 *  \brief A virtual class that receives decoded audio streams.
 *
 * \sa AudioOutputListeners
*/

AudioOutputListener::AudioOutputListener()
  : m_lock(new QMutex())
{
}

AudioOutputListener::~AudioOutputListener()
{
    delete m_lock;
    m_lock = NULL;
}

QMutex* AudioOutputListener::GetLock(void)
{
    return m_lock;
}

/*! \class AudioOutputListeners
 *  \brief Passes decoded audio data to a list of AudioOutputListener (s).
 *
 * \sa AudioOutputListener
*/

AudioOutputListeners::AudioOutputListeners()
  : m_lock(new QMutex()),
    m_bufferSize(0)
{
}

AudioOutputListeners::~AudioOutputListeners()
{
    delete m_lock;
    m_lock = NULL;
}

bool AudioOutputListeners::HasListeners(void)
{
    return m_listeners.size();
}

void AudioOutputListeners::AddListener(AudioOutputListener *Listener)
{
    if (!m_listeners.contains(Listener))
        m_listeners.append(Listener);
}

void AudioOutputListeners::RemoveListener(AudioOutputListener *Listener)
{
    if (m_listeners.contains(Listener))
        (void)m_listeners.removeAll(Listener);
}

void AudioOutputListeners::UpdateListeners(unsigned char *Buffer,
                                           unsigned long Length,
                                           unsigned long Written,
                                           int Channels,
                                           int Precision)
{
    if (!Buffer)
       return;

    QList<AudioOutputListener*>::iterator it = m_listeners.begin();
    for ( ; it != m_listeners.end(); ++it)
    {
        (*it)->GetLock()->lock();
        (*it)->AddSample(Buffer, Length, Written, Channels, Precision);
        (*it)->GetLock()->unlock();
    }
}

void AudioOutputListeners::PrepareListeners(void)
{
    QList<AudioOutputListener*>::iterator it = m_listeners.begin();
    for ( ; it != m_listeners.end(); ++it)
    {
        (*it)->GetLock()->lock();
        (*it)->Prepare();
        (*it)->GetLock()->unlock();
    }
}

QMutex* AudioOutputListeners::GetLock(void)
{
    return m_lock;
}

void AudioOutputListeners::SetBufferSize(unsigned int Size)
{
    m_bufferSize = Size;
}

unsigned int AudioOutputListeners::GetBufferSize(void) const
{
    return m_bufferSize;
}
