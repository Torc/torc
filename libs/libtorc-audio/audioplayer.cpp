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

// Qt
#include <QCoreApplication>
#include <QThread>

// Torc
#include "torclocalcontext.h"
#include "torclogging.h"
#include "torcdecoder.h"
#include "audiowrapper.h"
#include "audioplayer.h"

#define BLACKLIST QString("SetPropertyAvailable,SetPropertyUnavailable,StartRefreshTimer,StopRefreshTimer")

AudioPlayer::AudioPlayer(QObject *Parent, int PlaybackFlags, int DecodeFlags)
  : TorcPlayer(Parent, PlaybackFlags, DecodeFlags),
    TorcHTTPService(this, "player", tr("Player"), AudioPlayer::staticMetaObject, BLACKLIST),
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
    if (thread() != QThread::currentThread())
    {
        // turn this into an asynchronous call
        QVariantMap data;
        data.insert("uri", URI);
        data.insert("paused", Paused);
        TorcEvent *event = new TorcEvent(Torc::PlayMedia, data);
        QCoreApplication::postEvent(parent(), event);
        return true;
    }

    bool result = TorcPlayer::PlayMedia(URI, Paused);

    StartRefreshTimer(1000 / 50);

    return result;
}

void* AudioPlayer::GetAudio(void)
{
    return m_audioWrapper;
}

class AudioPlayerFactory : public PlayerFactory
{
    void Score(QObject *Parent, int PlaybackFlags, int DecoderFlags, int &Score)
    {
        if (!(DecoderFlags & TorcDecoder::DecodeVideo) && Score <= 10)
            Score = 10;
    }

    TorcPlayer* Create(QObject *Parent, int PlaybackFlags, int DecoderFlags, int &Score)
    {
        if (!(DecoderFlags & TorcDecoder::DecodeVideo) && Score <= 10)
            return new AudioPlayer(Parent, PlaybackFlags, DecoderFlags);

        return NULL;
    }
} AudioPlayerFactory;
