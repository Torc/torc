/* Class TorcBlurayUIBuffer
*
* This file is part of the Torc project.
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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Qt
#include <QMouseEvent>

// Torc
#include "torclogging.h"
#include "torcdecoder.h"
#include "torcplayer.h"
#include "torcblurayuihandler.h"
#include "torcblurayuibuffer.h"

/*! \class TorcBlurayUIBuffer
 *  \brief A subclasss of TorcBlurayBuffer that implements menus via TorcBlurayUIHandler.
*/
TorcBlurayUIBuffer::TorcBlurayUIBuffer(void *Parent, const QString &URI, int *Abort)
  : TorcBlurayBuffer(Parent, URI, Abort),
    m_uiHandler(NULL)
{
}

TorcBlurayUIBuffer::~TorcBlurayUIBuffer()
{
}

void TorcBlurayUIBuffer::InitialiseAVContext(void *Context)
{
    (void)Context;
    if (m_uiHandler)
        m_uiHandler->SetIgnoreWaits(false);
}

void* TorcBlurayUIBuffer::RequiredAVFormat(bool &BufferRequired)
{
    (void)BufferRequired;
    if (m_uiHandler)
        return m_uiHandler->GetAVInputFormat();

    return NULL;
}

///brief Open an appropriate handler for the bluray image.
bool TorcBlurayUIBuffer::Open(void)
{
    m_state = Status_Opening;

    m_handler = new TorcBlurayUIHandler(this, GetFilteredUri(), m_abort);
    if (m_handler->Open())
    {
        m_uiHandler = static_cast<TorcBlurayUIHandler*>(m_handler);
        return true;
    }

    delete m_handler;
    m_handler = NULL;

    m_state = Status_Invalid;
    return false;
}

/*! \brief Process events for libbluray interaction
 *
 * libbluray menus will accept interaction via both mouse and keyboard.
 *
 * \note Mouse events are filtered at the player level to detect clicks and a release event is
 *       interpreted as a click here.
*/
bool TorcBlurayUIBuffer::HandleEvent(QEvent *Event)
{
    if (!Event || !m_uiHandler)
        return false;

    if (QEvent::MouseButtonRelease == Event->type())
    {
        QMouseEvent *event = static_cast<QMouseEvent*>(Event);

        if (event)
            return m_uiHandler->MouseClicked(0/*m_videoPts*/, event->x(), event->y());
    }
    else if (QEvent::MouseMove == Event->type())
    {
        QMouseEvent *event = static_cast<QMouseEvent*>(Event);

        if (event)
            return m_uiHandler->MouseMoved(0/*m_videoPts*/, event->x(), event->y());
    }
    else if (QEvent::HoverMove == Event->type())
    {
        QHoverEvent *event = static_cast<QHoverEvent*>(Event);

        if (event)
            return m_uiHandler->MouseMoved(0/*m_videopts*/, event->pos().x(), event->pos().y());
    }

    return false;
}

/*! \class TorcBlurayUIBufferFactory
 *  \brief A static class to create an bluray disc buffer.
*/
static class TorcBlurayUIBufferFactory : public TorcBufferFactory
{
    void Score(const QString &URI, const QUrl &URL, int &Score, const bool &Media)
    {
        if (Media && URI.startsWith("bd:", Qt::CaseInsensitive) && Score <= 30)
            Score = 30;
    }

    TorcBuffer* Create(void *Parent, const QString &URI, const QUrl &URL, const int &Score, int *Abort, const bool &Media)
    {
        bool ui = false;
        TorcDecoder *parent = static_cast<TorcDecoder*>(Parent);
        if (parent)
        {
            TorcPlayer *player = parent->GetParent();
            if (player)
                ui = player->GetPlayerFlags() & TorcPlayer::UserFacing;
        }

        if (Media && URI.startsWith("bd:", Qt::CaseInsensitive) && Score <= 30)
        {
            if (ui)
                return new TorcBlurayUIBuffer(Parent, URI, Abort);
            else
                return new TorcBlurayBuffer(Parent, URI, Abort);
        }

        return NULL;
    }

} TorcBlurayUIBufferFactory;
