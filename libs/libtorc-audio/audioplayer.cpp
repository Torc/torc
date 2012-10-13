/* Class AudioPlayer
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

// Torc
#include "torclocalcontext.h"
#include "torclogging.h"
#include "torcmessage.h"
#include "torcdecoder.h"
#include "audiowrapper.h"
#include "audioplayer.h"

AudioPlayer::AudioPlayer(QObject *Parent, int PlaybackFlags, int DecodeFlags)
  : TorcPlayer(Parent, PlaybackFlags, DecodeFlags),
    m_audioWrapper(new AudioWrapper(this))
{
    setObjectName("Player");
}

AudioPlayer::~AudioPlayer()
{
    Teardown();

    delete m_audioWrapper;
    m_audioWrapper = NULL;
}

void AudioPlayer::Teardown(void)
{
    TorcPlayer::Teardown();
}

bool AudioPlayer::PlayMedia(const QString &URI, bool Paused)
{
    if (TorcPlayer::PlayMedia(URI, Paused) && !m_refreshTimer)
    {
        StartTimer(m_refreshTimer, 1000 / 50);
        return true;
    }

    return false;
}

void* AudioPlayer::GetAudio(void)
{
    return m_audioWrapper;
}

class AudioPlayerFactory : public PlayerFactory
{
    TorcPlayer* Create(QObject *Parent, int PlaybackFlags, int DecoderFlags)
    {
        if (DecoderFlags & TorcDecoder::DecodeVideo)
            return NULL;

        return new AudioPlayer(Parent, PlaybackFlags, DecoderFlags);
    }
} AudioPlayerFactory;
