/* Class UIMedia
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
#include "torclocalcontext.h"
#include "torclogging.h"
#include "torcevent.h"
#include "torcdecoder.h"
#include "uimedia.h"

int UIMedia::kUIMediaType = UIWidget::RegisterWidgetType();

static QScriptValue UIMediaConstructor(QScriptContext *context, QScriptEngine *engine)
{
    return WidgetConstructor<UIMedia>(context, engine);
}

UIMedia::UIMedia(UIWidget *Root, UIWidget *Parent, const QString &Name, int Flags)
  : UIWidget(Root, Parent, Name, Flags | WidgetFlagFocusable),
    TorcPlayerInterface(false)
{
    m_type = kUIMediaType;

    if (m_parent && !m_template)
        m_parent->IncreaseFocusableChildCount();

    gLocalContext->AddObserver(this);
}

UIMedia::~UIMedia()
{
    gLocalContext->RemoveObserver(this);

    if (m_parent && !m_template)
        m_parent->DecreaseFocusableChildCount();

    delete m_player;
    m_player = NULL;
}

bool UIMedia::HandleAction(int Action)
{
    if (HandlePlayerAction(Action))
        return true;

    switch (Action)
    {
        default:
            break;
    }

    if (m_parent)
        return m_parent->HandleAction(Action);

    return false;
}

bool UIMedia::HandlePlayerAction(int Action)
{
    return TorcPlayerInterface::HandlePlayerAction(Action);
}

void UIMedia::CopyFrom(UIWidget *Other)
{
    UIMedia *media = dynamic_cast<UIMedia*>(Other);

    if (!media)
    {
        LOG(LOG_INFO, LOG_ERR, "Failed to cast widget to UIMedia.");
        return;
    }

    UIWidget::CopyFrom(Other);
}

bool UIMedia::Draw(quint64 TimeNow, UIWindow *Window, qreal XOffset, qreal YOffset)
{
    if (m_player)
        m_player->Refresh();

    return UIWidget::Draw(TimeNow, Window, XOffset, YOffset);
}

bool UIMedia::Finalise(void)
{
    if (m_template)
        return true;

    InitialisePlayer();
    return UIWidget::Finalise();
}

bool UIMedia::InitialisePlayer(void)
{
    m_player = TorcPlayer::Create(this, TorcPlayer::UserFacing, TorcDecoder::DecodeVideo | TorcDecoder::DecodeAudio);

    if (m_player)
    {
        connect(m_player, SIGNAL(StateChanged(TorcPlayer::PlayerState)),
                this, SLOT(PlayerStateChanged(TorcPlayer::PlayerState)));
    }

    return m_player;
}

bool UIMedia::InitialisePriv(QDomElement *Element)
{
    return UIWidget::InitialisePriv(Element);
}

bool UIMedia::event(QEvent *Event)
{
    if (Event->type() == TorcEvent::TorcEventType)
        return HandleEvent(Event);

    return false;
}

void UIMedia::CreateCopy(UIWidget *Parent)
{
    UIMedia* media = new UIMedia(m_rootParent, Parent,
                                 GetDerivedWidgetName(Parent->objectName()),
                                 WidgetFlagNone);
    media->CopyFrom(this);
}

void UIMedia::PlayerStateChanged(TorcPlayer::PlayerState NewState)
{
    LOG(VB_GENERAL, LOG_INFO, QString("Player state '%1'").arg(TorcPlayer::StateToString(NewState)));
}

static class UIMediaFactory : public WidgetFactory
{
    void RegisterConstructor(QScriptEngine *Engine)
    {
        if (!Engine)
            return;

        QScriptValue constructor = Engine->newFunction(UIMediaConstructor);
        Engine->globalObject().setProperty("UIMedia", constructor);
    }

    UIWidget* Create(const QString &Type, UIWidget *Root, UIWidget *Parent,
                     const QString &Name, int Flags)
    {
        if (Type == "media")
            return new UIMedia(Root, Parent, Name, Flags);
        return NULL;
    }

} UIMediaFactory;
