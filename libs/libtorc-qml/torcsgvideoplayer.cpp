/* Class TorcSGVideoPlayer
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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

// Torc
#include "torclogging.h"
#include "audiowrapper.h"
#include "videodecoder.h"
#include "videoframe.h"
#include "videocolourspace.h"
#include "torcsgvideoprovider.h"
#include "torcsgvideoplayer.h"

/*! \class TorcSGVideoPlayer
 *  \brief The TorcPlayer subclass for presenting media through a UI.
 *
 * \sa TorcPlayer
 * \sa VideoPlayer
 *
 * \todo Review TorcPlayer interface for changes post move to QML (e.g. Render)
*/
TorcSGVideoPlayer::TorcSGVideoPlayer(QObject *Parent, int PlaybackFlags, int DecodeFlags)
  : VideoPlayer(Parent, PlaybackFlags, DecodeFlags),
    m_resetVideoProvider(false),
    m_videoProvider(NULL),
    m_currentFrame(NULL),
    m_manualAVSyncAdjustment(0),
    m_waitState(WaitStateNone)
{
    connect(this, SIGNAL(ResetRequest()), this, SLOT(HandleReset()));
}

TorcSGVideoPlayer::~TorcSGVideoPlayer()
{
    Teardown();
}

void TorcSGVideoPlayer::Teardown(void)
{
    if (m_currentFrame)
        m_buffers.ReleaseFrameFromDisplaying(m_currentFrame, false);
    m_currentFrame = NULL;

    VideoPlayer::Teardown();
}

bool TorcSGVideoPlayer::HandleAction(int Action)
{
    return VideoPlayer::HandleAction(Action);
}

void TorcSGVideoPlayer::SetVideoProvider(TorcSGVideoProvider *Provider)
{
    if (m_videoProvider)
        LOG(VB_GENERAL, LOG_INFO, "Setting new video provider");

    m_videoProvider = Provider;
}

void TorcSGVideoPlayer::Render(quint64 TimeNow)
{
    (void)TimeNow;
}

void TorcSGVideoPlayer::Reset(void)
{
    emit ResetRequest();
}

void TorcSGVideoPlayer::HandleReset(void)
{
    if (m_currentFrame)
        m_buffers.ReleaseFrameFromDisplaying(m_currentFrame, false);
    m_currentFrame = NULL;

    // this needs to be processed in the Qt render thread
    m_resetVideoProvider = true;

    VideoPlayer::Reset();
}

/*! \brief Refresh the currently playing media.
 *
 * For audio only files, this is a no-op as the audio thread will keep the audio playing at
 * the appropriate rate. For audio and video, this method attempts to synchronise the current
 * video frame to the current audio timestamp.
 *
 * \todo Video only playback timing.
 * \todo Add back EDID adjustments.
*/
bool TorcSGVideoPlayer::Refresh(quint64 TimeNow, const QSizeF &Size, bool Visible)
{
    if (m_currentFrame)
        m_buffers.ReleaseFrameFromDisplaying(m_currentFrame, false);
    m_currentFrame = NULL;

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
                //int edidajustment = UIEDID::GetVideoLatency(false/*interlaced*/) - UIEDID::GetAudioLatency(false/*interlaced*/);
                //audiotime += edidajustment;

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
            if (m_waitState != WaitStateVideoAhead)
            {
                m_waitTimer.restart();
                m_waitState = WaitStateVideoAhead;
            }

            if (m_waitTimer.elapsed() > 250)
            {
                LOG(VB_GENERAL, LOG_INFO, QString("Video ahead of audio by %1ms - waiting").arg(videotime - audiotime));
                m_waitTimer.restart();
            }
        }
        else if (hasaudiostream && !validaudio)
        {
            if (m_waitState != WaitStateNoAudio)
            {
                m_waitTimer.restart();
                m_waitState = WaitStateNoAudio;
            }

            if (m_waitTimer.elapsed() > 250)
            {
                LOG(VB_GENERAL, LOG_INFO, "Waiting for audio to start");
                m_waitTimer.restart();
            }
        }
        else if (hasaudiostream && !validvideo)
        {
            if (m_waitState != WaitStateNoVideo)
            {
                m_waitTimer.restart();
                m_waitState = WaitStateNoVideo;
            }

            if (m_waitTimer.elapsed() > 250)
            {
                LOG(VB_GENERAL, LOG_INFO, "Waiting for video to start");
                m_waitTimer.restart();
            }
        }
        else
        {
            m_waitState = WaitStateNone;
            m_waitTimer.invalidate();

            // get the next available frame. If no video this will be null
            m_currentFrame = m_buffers.GetFrameForDisplaying();

            // sync audio and video - if we have both
            if (m_currentFrame && hasaudiostream)
            {
                videotime = m_currentFrame->m_pts;
                qint64 drift = audiotime - videotime;

                LOG(VB_GENERAL, LOG_DEBUG, QString("AVSync: %1").arg(drift));

                while (drift > 50)
                {
                    LOG(VB_GENERAL, LOG_INFO, QString("Audio ahead of video by %1ms - dropping frame %2")
                        .arg(drift).arg(m_currentFrame->m_frameNumber));
                    m_buffers.ReleaseFrameFromDisplaying(m_currentFrame, false);
                    m_currentFrame = m_buffers.GetFrameForDisplaying();

                    if (m_currentFrame)
                    {
                        videotime = m_currentFrame->m_pts;
                        drift = audiotime - videotime;
                        continue;
                    }

                    break;
                }
            }
        }

        if (m_currentFrame && m_videoProvider)
        {
            if (m_state == Paused  || m_state == Starting ||
                m_state == Playing || m_state == Searching ||
                m_state == Pausing || m_state == Stopping)
            {
                m_videoProvider->Refresh(m_currentFrame, Size, TimeNow, m_resetVideoProvider);
                m_resetVideoProvider = false;
            }
        }
    }

    // check whether the demuxer was waiting for the buffers to drain (Bluray, DVD etc)
    if (m_decoder && m_buffers.GetNumberReadyFrames() < 1)
    {
        VideoDecoder *decoder = static_cast<VideoDecoder*>(m_decoder);
        if (decoder && decoder->GetDemuxerState() != TorcDecoder::DemuxerReady)
        {
            LOG(VB_GENERAL, LOG_INFO, "No more video frames - unpausing demuxer");

            // NB the demuxer may flush itself once back in the 'ready' state, so release current frame
            if (m_currentFrame)
                m_buffers.ReleaseFrameFromDisplaying(m_currentFrame, false);
            m_currentFrame = NULL;

            // release the demuxer again
            decoder->SetDemuxerState(TorcDecoder::DemuxerReady);

            // and enusure a smooth av sync for the new sequence
            m_audioWrapper->ClearAudioTime();
        }
    }

    return TorcPlayer::Refresh(TimeNow, Size, Visible) && (m_currentFrame != NULL);
}

class TorcSGVideoPlayerFactory : public PlayerFactory
{
    void Score(QObject *Parent, int PlaybackFlags, int DecoderFlags, int &Score)
    {
        if ((DecoderFlags & TorcDecoder::DecodeVideo) && (PlaybackFlags & TorcPlayer::UserFacing) && Score <= 20)
            Score = 20;
    }

    TorcPlayer* Create(QObject *Parent, int PlaybackFlags, int DecoderFlags, int &Score)
    {
        if ((DecoderFlags & TorcDecoder::DecodeVideo) && (PlaybackFlags & TorcPlayer::UserFacing) && Score <= 20)
            return new TorcSGVideoPlayer(Parent, PlaybackFlags, DecoderFlags);

        return NULL;
    }
} TorcSGVideoPlayerFactory;
