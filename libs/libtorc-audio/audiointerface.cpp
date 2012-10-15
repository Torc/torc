/* Class AudioInterface
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

// Torc
#include "torclocalcontext.h"
#include "torcdecoder.h"
#include "audioplayer.h"
#include "audiointerface.h"

AudioInterface::AudioInterface(QObject *Parent, bool Standalone)
  : QObject(Parent),
    TorcPlayerInterface(Standalone)
{
    gLocalContext->AddObserver(this);
}

AudioInterface::~AudioInterface()
{
    gLocalContext->RemoveObserver(this);
    delete m_player;
    m_player = NULL;
}

bool AudioInterface::event(QEvent *Event)
{
    if (Event->type() == TorcEvent::TorcEventType)
        return HandleEvent(Event);

    return false;
}

bool AudioInterface::InitialisePlayer(void)
{
    m_player = TorcPlayer::Create(this, TorcPlayer::UserFacing, TorcDecoder::DecodeAudio);

    if (m_player)
    {
        connect(m_player, SIGNAL(StateChanged(TorcPlayer::PlayerState)),
                this, SLOT(PlayerStateChanged(TorcPlayer::PlayerState)));
    }

    return m_player;
}

void AudioInterface::PlayerStateChanged(TorcPlayer::PlayerState NewState)
{
    LOG(VB_GENERAL, LOG_INFO, QString("Player state '%1'").arg(TorcPlayer::StateToString(NewState)));

    if ((NewState == TorcPlayer::Stopped || NewState == TorcPlayer::Errored) && !parent())
        QCoreApplication::quit();
}
