/* Class VideoPlayer
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
#include "torclogging.h"
#include "torcdecoder.h"
#include "videoframe.h"
#include "videoplayer.h"

VideoPlayer::VideoPlayer(QObject *Parent, int PlaybackFlags, int DecodeFlags)
  : TorcPlayer(Parent, PlaybackFlags, DecodeFlags),
    m_audioWrapper(new AudioWrapper(this)),
    m_reset(false)
{
    setObjectName("Player");

    m_buffers.SetDisplayFormat(PIX_FMT_YUV420P);
}

VideoPlayer::~VideoPlayer()
{
    Teardown();

    delete m_audioWrapper;
    m_audioWrapper = NULL;
}

void VideoPlayer::Teardown(void)
{
    TorcPlayer::Teardown();
}

bool VideoPlayer::Refresh(quint64 TimeNow)
{
    VideoFrame *frame = m_buffers.GetFrameForDisplaying();
    if (frame)
        m_buffers.ReleaseFrameFromDisplaying(frame, false);

    return TorcPlayer::Refresh(TimeNow);
}

void VideoPlayer::Render(quint64 TimeNow)
{
    (void)TimeNow;
}

void VideoPlayer::Reset(void)
{
    m_buffers.Reset(true);
    m_reset = false;
}

void* VideoPlayer::GetAudio(void)
{
    return m_audioWrapper;
}

VideoBuffers* VideoPlayer::Buffers(void)
{
    return &m_buffers;
}

class VideoPlayerFactory : public PlayerFactory
{
    TorcPlayer* Create(QObject *Parent, int PlaybackFlags, int DecoderFlags)
    {
        if ((DecoderFlags & TorcDecoder::DecodeVideo) && !(PlaybackFlags & TorcPlayer::UserFacing))
            return new VideoPlayer(Parent, PlaybackFlags, DecoderFlags);

        return NULL;
    }
} VideoPlayerFactory;
