/* Class VideoPlayer
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
#include "torcadminthread.h"
#include "torclogging.h"
#include "torcdecoder.h"
#include "videoframe.h"
#include "videoplayer.h"

TorcSetting* VideoPlayer::gEnableAcceleration = NULL;
TorcSetting* VideoPlayer::gAllowGPUAcceleration = NULL;
TorcSetting* VideoPlayer::gAllowOtherAcceleration = NULL;

/*! \class TorcVideoPlayerSettings
 *  \brief Creates the video playback settings
 *
 * TorcPlayer::gVideoSettings is created by TorcPlayerSettings (with high priority) so
 * is already extant when this class is called.
 *
 * \sa TorcSetting
 * \sa TorcPlayer
 * \sa TorcPlayerSettings
*/
class TorcVideoPlayerSettings : public TorcAdminObject
{
  public:
    TorcVideoPlayerSettings()
      : TorcAdminObject(TORC_ADMIN_MED_PRIORITY),
        m_created(false)
    {
    }

    void Create()
    {
        if (!TorcPlayer::gVideoSettings)
            return;

        VideoPlayer::gEnableAcceleration = new TorcSetting(TorcPlayer::gVideoSettings,
                                                          TORC_VIDEO + "AllowAcceleration",
                                                          QObject::tr("Enable accelerated video decoding"),
                                                          TorcSetting::Checkbox, true, QVariant((bool)true));

        // allow crystalhd/vda etc
        VideoPlayer::gAllowOtherAcceleration = new TorcSetting(VideoPlayer::gEnableAcceleration,
                                                               TORC_VIDEO + "AllowOtherAcceleration",
                                                               QObject::tr("Allow non GPU video acceleration"),
                                                               TorcSetting::Checkbox, true, QVariant((bool)true));
        // GPU based acceleration
        VideoPlayer::gAllowGPUAcceleration = new TorcSetting(VideoPlayer::gEnableAcceleration,
                                                             TORC_VIDEO + "AllowGPUAcceleration",
                                                             QObject::tr("Allow GPU video acceleration"),
                                                             TorcSetting::Checkbox, true, QVariant((bool)true));

        VideoPlayer::gEnableAcceleration->SetActive(true);
        // default to no GPU based acceleration for non-UI applications.
        // VideoUIPlayer (or other) will call SetActive(true)
        VideoPlayer::gAllowGPUAcceleration->SetActiveThreshold(2);
        // but allow acceleration that returns a software frame (VDA, crystalhd etc)
        VideoPlayer::gAllowOtherAcceleration->SetActiveThreshold(1);

        // setup dependencies
        if (VideoPlayer::gEnableAcceleration->GetValue().toBool())
        {
            VideoPlayer::gAllowOtherAcceleration->SetActive(true);
            VideoPlayer::gAllowGPUAcceleration->SetActive(true);
        }

        QObject::connect(VideoPlayer::gEnableAcceleration, SIGNAL(ValueChanged(bool)), VideoPlayer::gAllowOtherAcceleration, SLOT(SetActive(bool)));
        QObject::connect(VideoPlayer::gEnableAcceleration, SIGNAL(ValueChanged(bool)), VideoPlayer::gAllowGPUAcceleration, SLOT(SetActive(bool)));

        m_created = true;
    }

    void Destroy(void)
    {
        if (m_created)
        {
            VideoPlayer::gAllowOtherAcceleration->Remove();
            VideoPlayer::gAllowOtherAcceleration->DownRef();
            VideoPlayer::gAllowGPUAcceleration->Remove();
            VideoPlayer::gAllowGPUAcceleration->DownRef();
            VideoPlayer::gEnableAcceleration->Remove();
            VideoPlayer::gEnableAcceleration->DownRef();
        }

        VideoPlayer::gEnableAcceleration = NULL;
        VideoPlayer::gAllowOtherAcceleration = NULL;
        VideoPlayer::gAllowGPUAcceleration = NULL;
    }

  private:
    bool m_created;

} TorcVideoPlayerSettings;

VideoPlayer::VideoPlayer(QObject *Parent, int PlaybackFlags, int DecodeFlags)
  : TorcPlayer(Parent, PlaybackFlags, DecodeFlags),
    m_audioWrapper(new AudioWrapper(this)),
    m_reset(false)
{
    setObjectName("Player");

    m_buffers.SetDisplayFormat(AV_PIX_FMT_YUV420P);
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

bool VideoPlayer::Refresh(quint64 TimeNow, const QSizeF &Size, bool Visible)
{
    VideoFrame *frame = m_buffers.GetFrameForDisplaying();
    if (frame)
        m_buffers.ReleaseFrameFromDisplaying(frame, false);

    return TorcPlayer::Refresh(TimeNow, Size, Visible);
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

VideoBuffers* VideoPlayer::GetBuffers(void)
{
    return &m_buffers;
}

QVariant VideoPlayer::GetProperty(PlayerProperty Property)
{
    return TorcPlayer::GetProperty(Property);
}

void VideoPlayer::SetProperty(PlayerProperty Property, QVariant Value)
{
    TorcPlayer::SetProperty(Property, Value);
}

class VideoPlayerFactory : public PlayerFactory
{
    void Score(QObject *Parent, int PlaybackFlags, int DecoderFlags, int &Score)
    {
        if ((DecoderFlags & TorcDecoder::DecodeVideo) && !(PlaybackFlags & TorcPlayer::UserFacing) && Score <= 10)
            Score = 10;
    }

    TorcPlayer* Create(QObject *Parent, int PlaybackFlags, int DecoderFlags, int &Score)
    {
        if ((DecoderFlags & TorcDecoder::DecodeVideo) && !(PlaybackFlags & TorcPlayer::UserFacing) && Score <= 10)
            return new VideoPlayer(Parent, PlaybackFlags, DecoderFlags);

        return NULL;
    }
} VideoPlayerFactory;
