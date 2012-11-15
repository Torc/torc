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
    TorcHTTPService(this, "/player", tr("Player"), VideoPlayer::staticMetaObject),
    m_audioWrapper(new AudioWrapper(this))
{
    setObjectName("Player");
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

void VideoPlayer::Refresh(void)
{
    VideoFrame *frame = m_buffers.GetFrameForDisplaying();
    if (frame)
    {
        //LOG(VB_GENERAL, LOG_INFO, QString("AV %1").arg(m_audioWrapper->GetAudioTime() - frame->m_pts));
        m_buffers.ReleaseFrameFromDisplaying(frame, false);
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, "Skip");
    }

    TorcPlayer::Refresh();
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
        if (DecoderFlags & TorcDecoder::DecodeVideo)
            return new VideoPlayer(Parent, PlaybackFlags, DecoderFlags);

        return NULL;
    }
} VideoPlayerFactory;
