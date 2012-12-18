/* Class VideoUIPlayer
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
#include "torcconfig.h"
#include "torcthread.h"
#include "torcdecoder.h"
#include "videoframe.h"
#include "videorenderer.h"
#include "videocolourspace.h"
#include "videouiplayer.h"

#if CONFIG_X11BASE
#if CONFIG_VDPAU
#include "videovdpau.h"
#endif
#if CONFIG_VAAPI
#include "videovaapi.h"
#endif
#endif

void VideoUIPlayer::Initialise(void)
{
#if CONFIG_X11BASE
#if CONFIG_VDPAU
    (void)VideoVDPAU::VDPAUAvailable();
#endif
#if CONFIG_VAAPI
    (void)VideoVAAPI::VAAPIAvailable();
#endif
#endif
}

VideoUIPlayer::VideoUIPlayer(QObject *Parent, int PlaybackFlags, int DecodeFlags)
  : VideoPlayer(Parent, PlaybackFlags, DecodeFlags),
    TorcHTTPService(this, "/player", tr("Player"), VideoPlayer::staticMetaObject),
    m_colourSpace(new VideoColourSpace(AVCOL_SPC_UNSPECIFIED))
{
    m_render = VideoRenderer::Create(m_colourSpace);
    m_buffers.SetDisplayFormat(m_render ? m_render->PreferredPixelFormat() : PIX_FMT_YUV420P);
}

VideoUIPlayer::~VideoUIPlayer()
{
    Teardown();
    delete m_colourSpace;
}

void VideoUIPlayer::Teardown(void)
{
    VideoPlayer::Teardown();
}

bool VideoUIPlayer::Refresh(quint64 TimeNow)
{
    if (m_reset)
        Reset();

    VideoFrame *frame = m_buffers.GetFrameForDisplaying();

    if (m_render)
    {
        if (m_state == Paused  || m_state == Starting ||
            m_state == Playing || m_state == Searching ||
            m_state == Pausing || m_state == Stopping)
        {
            m_render->RefreshFrame(frame);
        }
    }

    if (frame)
        m_buffers.ReleaseFrameFromDisplaying(frame, false);

    return TorcPlayer::Refresh(TimeNow) && (frame != NULL);
}

void VideoUIPlayer::Render(quint64 TimeNow)
{
    if (m_render)
        m_render->RenderFrame();
}

void VideoUIPlayer::Reset(void)
{
    if (TorcThread::IsMainThread())
    {
        m_colourSpace->SetChanged();
        if (m_render)
            m_render->PlaybackFinished();
        VideoPlayer::Reset();
    }
    else
    {
        m_reset = true;
    }
}

class VideoUIPlayerFactory : public PlayerFactory
{
    TorcPlayer* Create(QObject *Parent, int PlaybackFlags, int DecoderFlags)
    {
        if ((DecoderFlags & TorcDecoder::DecodeVideo) && (PlaybackFlags & TorcPlayer::UserFacing))
            return new VideoUIPlayer(Parent, PlaybackFlags, DecoderFlags);

        return NULL;
    }
} VideoUIPlayerFactory;

