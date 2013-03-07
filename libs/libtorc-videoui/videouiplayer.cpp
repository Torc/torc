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
#include "uiedid.h"
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

/*! \class VideoUIPlayer
 *  \brief The default media player for presenting audio and/or video within the GUI
 *
 * \todo Interlaced video
 * \todo Interlaced audio/videolatency from edid
 * \todo Detect/guess whether audio and video are provided over the same interface (HDMI)
 * \todo Timecode wrap detection and handling
 * \todo Timecode distcontinuity detection and handling
 * \todo repeat_picture handling
 * \todo Discard early audio samples
 * \todo Dynamic drift size
 */


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
    m_manualAVSyncAdjustment = gLocalContext->GetSetting(TORC_GUI + "AVSyncAdj", (int)0);
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

bool VideoUIPlayer::Refresh(quint64 TimeNow, const QSizeF &Size)
{
    if (m_reset)
        Reset();

    VideoFrame *frame = NULL;

    if (m_decoder && (m_decoder->GetCurrentStream(StreamTypeVideo) != -1))
    {
        bool hasaudiostream = m_decoder->GetCurrentStream(StreamTypeAudio) != -1;

        // get audio time. If no audio will be AV_NOPTS_VALUE
        quint64 lastaudioupdate = 0;
        qint64 audiotime = AV_NOPTS_VALUE;
        if (m_audioWrapper && hasaudiostream)
        {
            audiotime = m_audioWrapper->GetAudioTime(lastaudioupdate);

            // update audio time - audiotime is milliseconds, TimeNow is microseconds
            if (audiotime != (qint64)AV_NOPTS_VALUE)
            {
                lastaudioupdate = lastaudioupdate < TimeNow ? TimeNow - lastaudioupdate : 0;
                audiotime += lastaudioupdate / 1000;

                // adjust for known latencies in video and audio rendering (EDID values are milliseconds)
                int edidajustment = UIEDID::GetVideoLatency(false/*interlaced*/) - UIEDID::GetAudioLatency(false/*interlaced*/);
                audiotime += edidajustment;

                // user manual adjustment - milliseconds
                audiotime += m_manualAVSyncAdjustment;
            }
        }

        // get video time
        qint64 videotime = AV_NOPTS_VALUE;
        m_buffers.GetNextVideoTimeStamp(videotime);

        bool validaudio = audiotime != (qint64)AV_NOPTS_VALUE;
        bool validvideo = videotime != (qint64)AV_NOPTS_VALUE;

        LOG(VB_GENERAL, LOG_DEBUG, QString("A:%1 V:%2").arg(audiotime).arg(videotime));

        if (hasaudiostream && validaudio && validvideo && ((videotime - audiotime) > 50))
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Video ahead of audio by %1ms - waiting").arg(videotime - audiotime));
        }
        else if (hasaudiostream && !validaudio)
        {
            LOG(VB_GENERAL, LOG_INFO, "Waiting for audio to start");
        }
        else if (hasaudiostream && !validvideo)
        {
            LOG(VB_GENERAL, LOG_INFO, "Waiting for video to start");
        }
        else
        {
            // get the next available frame. If no video this will be null
            frame = m_buffers.GetFrameForDisplaying();

            // sync audio and video - if we have both
            if (frame && hasaudiostream)
            {
                videotime = frame->m_pts;
                qint64 drift = audiotime - videotime;

                LOG(VB_GENERAL, LOG_DEBUG, QString("AVSync: %1").arg(drift));

                while (drift > 50)
                {
                    LOG(VB_GENERAL, LOG_INFO, QString("Audio ahead of video by %1ms - dropping frame %2")
                        .arg(drift).arg(frame->m_frameNumber));
                    m_buffers.ReleaseFrameFromDisplaying(frame, false);
                    frame = m_buffers.GetFrameForDisplaying();

                    if (frame)
                    {
                        videotime = frame->m_pts;
                        drift = audiotime - videotime;
                        continue;
                    }

                    break;
                }
            }
        }

        if (m_render)
        {
            if (m_state == Paused  || m_state == Starting ||
                m_state == Playing || m_state == Searching ||
                m_state == Pausing || m_state == Stopping)
            {
                m_render->RefreshFrame(frame, Size);
            }
        }

        if (frame)
            m_buffers.ReleaseFrameFromDisplaying(frame, false);
    }

    return TorcPlayer::Refresh(TimeNow, Size) && (frame != NULL);
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

bool VideoUIPlayer::HandleAction(int Action)
{
    if (m_render)
    {
        if (Action == Torc::DisplayDeviceReset)
        {
            return m_render->DisplayReset();
        }
        else if (Action == Torc::EnableHighQualityScaling ||
                 Action == Torc::DisableHighQualityScaling ||
                 Action == Torc::ToggleHighQualityScaling)
        {
            if (m_render->HighQualityScalingAllowed())
            {
                if (Action == Torc::EnableHighQualityScaling)
                {
                    SendUserMessage(QObject::tr("Requested high quality scaling"));
                    return m_render->SetHighQualityScaling(true);
                }
                else if (Action == Torc::DisableHighQualityScaling)
                {
                    SendUserMessage(QObject::tr("Disabled high quality scaling"));
                    return m_render->SetHighQualityScaling(false);
                }
                else if (Action == Torc::ToggleHighQualityScaling)
                {
                    bool enabled = !m_render->GetHighQualityScaling();
                    SendUserMessage(enabled ? QObject::tr("Requested high quality scaling") :
                                              QObject::tr("Disabled high quality scaling"));
                    return m_render->SetHighQualityScaling(enabled);
                }
            }
            else
            {
                SendUserMessage(QObject::tr("Not available"));
            }
        }
    }

    return VideoPlayer::HandleAction(Action);
}

class VideoUIPlayerFactory : public PlayerFactory
{
    void Score(QObject *Parent, int PlaybackFlags, int DecoderFlags, int &Score)
    {
        if ((DecoderFlags & TorcDecoder::DecodeVideo) && (PlaybackFlags & TorcPlayer::UserFacing) && Score <= 20)
            Score = 20;
    }

    TorcPlayer* Create(QObject *Parent, int PlaybackFlags, int DecoderFlags, int &Score)
    {
        if ((DecoderFlags & TorcDecoder::DecodeVideo) && (PlaybackFlags & TorcPlayer::UserFacing) && Score <= 20)
            return new VideoUIPlayer(Parent, PlaybackFlags, DecoderFlags);

        return NULL;
    }
} VideoUIPlayerFactory;

