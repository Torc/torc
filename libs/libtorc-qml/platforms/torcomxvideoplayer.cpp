/* Class TorcOMXVideoPlayer
*
* Copyright (C) Mark Kendall 5013
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
#include "torcomxvideoplayer.h"

/*! \class TorcOMXVideoPlayer
 *  \brief Video player for OpenMax accelerated playback.
 *
 * \sa TorcSGVideoPlayer
 *
 * \todo Everything.
*/
TorcOMXVideoPlayer::TorcOMXVideoPlayer(QObject *Parent, int PlaybackFlags, int DecodeFlags)
  : TorcSGVideoPlayer(Parent, PlaybackFlags, DecodeFlags)
{
}

TorcOMXVideoPlayer::~TorcOMXVideoPlayer()
{
}

void TorcOMXVideoPlayer::Render(quint64 TimeNow)
{
    (void)TimeNow;
}

bool TorcOMXVideoPlayer::Refresh(quint64 TimeNow, const QSizeF &Size, bool Visible)
{
    // TODO Add buffer drain code

    return TorcPlayer::Refresh(TimeNow, Size, Visible);
}

class TorcOMXVideoPlayerFactory : public PlayerFactory
{
    void Score(QObject *Parent, int PlaybackFlags, int DecoderFlags, int &Score)
    {
        if ((DecoderFlags & TorcDecoder::DecodeVideo) && (PlaybackFlags & TorcPlayer::UserFacing) && Score <= 50)
            Score = 50;
    }

    TorcPlayer* Create(QObject *Parent, int PlaybackFlags, int DecoderFlags, int &Score)
    {
        if ((DecoderFlags & TorcDecoder::DecodeVideo) && (PlaybackFlags & TorcPlayer::UserFacing) && Score <= 50)
            return new TorcOMXVideoPlayer(Parent, PlaybackFlags, DecoderFlags);

        return NULL;
    }
} TorcOMXVideoPlayerFactory;
