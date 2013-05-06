/* Class UIButton
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

// Qt
#include <QtScript>
#include <QDomElement>

// Torc
#include "torclogging.h"
#include "torclocalcontext.h"
#include "uianimation.h"
#include "uitext.h"
#include "uibutton.h"

int UIButton::kUIButtonType = UIWidget::RegisterWidgetType();

static QScriptValue UIButtonConstructor(QScriptContext *context, QScriptEngine *engine)
{
    return WidgetConstructor<UIButton>(context, engine);
}

UIButton::UIButton(UIWidget *Root, UIWidget *Parent, const QString &Name, int Flags)
  : UIWidget(Root, Parent, Name, Flags | WidgetFlagFocusable),
    m_pushed(false),
    m_checkbox(false),
    m_toggled(0),
    m_textWidget(NULL),
    m_action(Torc::None),
    m_actionReceiver(NULL)
{
    m_type = kUIButtonType;

    if (m_parent && !m_template)
        m_parent->IncreaseFocusableChildCount();
}

UIButton::~UIButton()
{
    if (m_parent && !m_template)
        m_parent->DecreaseFocusableChildCount();
}

bool UIButton::InitialisePriv(QDomElement *Element)
{
    if (!Element)
        return false;

    if (Element->tagName() == "state")
    {
        // NB don't return true to allow UIWidget to parse base state
        QString pushed = Element->attribute("pushed");
        QString focus  = Element->attribute("focusable");

        if (!pushed.isEmpty())
            SetPushed(GetBool(pushed));

        if (!focus.isEmpty())
            SetFocusable(GetBool(focus));
    }
    else if (Element->tagName() == "message")
    {
        text = UIWidget::GetText(Element);
        return true;
    }
    else if (Element->tagName() == "checkbox")
    {
        SetAsCheckbox(true);
        return true;
    }

    return false;
}

bool UIButton::Finalise(void)
{
    if (m_template)
        return true;

    UIWidget* child = FindChildByName("text");

    if (child && child->Type() == UIText::kUITextType)
    {
        m_textWidget = dynamic_cast<UIText*>(child);
        SetText(text);
    }
    else
    {
        m_textWidget = NULL;
    }

    return UIWidget::Finalise();
}

bool UIButton::HandleAction(int Action)
{
    switch (Action)
    {
        case Torc::Pushed:
            Push();
            return true;
        case Torc::Released:
            Release();
            return true;
        default:
            break;
    }

    if (m_parent)
        return m_parent->HandleAction(Action);

    return false;
}

void UIButton::CopyFrom(UIWidget *Other)
{
    UIButton *button = dynamic_cast<UIButton*>(Other);

    if (!button)
    {
        LOG(LOG_INFO, LOG_ERR, "Failed to cast widget to UIButton.");
        return;
    }

    SetPushed(button->IsPushed());
    SetAsCheckbox(button->m_checkbox);
    text = button->GetText();

    // NB IsFocusable returns false for a template
    m_focusable = button->m_focusable;

    UIWidget::CopyFrom(Other);
}

UIWidget* UIButton::CreateCopy(UIWidget *Parent, const QString &Newname)
{
    bool isnew = !Newname.isEmpty();
    UIButton* button = new UIButton(m_rootParent, Parent,
                                    isnew ? Newname : GetDerivedWidgetName(Parent->objectName()),
                                    (!isnew && IsTemplate()) ? WidgetFlagTemplate : WidgetFlagNone);
    button->CopyFrom(this);
    return button;
}

void UIButton::AutoConnect(void)
{
    foreach (UIWidget* child, m_children)
    {
        if (child->Type() != UIAnimation::kUIAnimationType)
            continue;

        QString name = child->objectName();
        if (!name.startsWith(objectName()))
            continue;

        QString signal = name.mid(objectName().size() + 1);

        if (signal == "Pushed" || signal == "Released")
        {
            Connect("", signal, name, "Start");
        }
    }

    UIWidget::AutoConnect();
}

void UIButton::SetAction(int Action)
{
    m_action = Action;
}

void UIButton::SetReceiver(const QString &Receiver)
{
    if (m_rootParent)
    {
        UIWidget *receiver = m_rootParent->FindWidget(Receiver);
        if (receiver)
            m_actionReceiver = receiver;
        else
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to find receiver '%1'").arg(Receiver));
    }
}

void UIButton::SetReceiver(QObject *Receiver)
{
    m_actionReceiver = Receiver;
}

void UIButton::SetActionData(const QVariantMap &Data)
{
    m_actionData = Data;
}

void UIButton::SetAsCheckbox(bool Checkbox)
{
    m_checkbox = Checkbox;
}

bool UIButton::IsPushed(void)
{
    return m_pushed;
}

QString UIButton::GetText(void)
{
    if (m_textWidget)
        return m_textWidget->GetText();
    return QString("");
}

void UIButton::SetPushed(bool Pushed)
{
    if (Pushed)
        Push();
    else
        Release();
}

void UIButton::SetText(QString Text)
{
    if (m_textWidget)
        m_textWidget->SetText(Text);
}

void UIButton::Push(void)
{
    if (m_checkbox && m_toggled)
        return;

    if (!m_focusable)
        LOG(VB_GENERAL, LOG_WARNING, "Pushing button that isn't focusable");

    bool signal = !m_pushed;
    m_pushed = true;

    if (signal)
    {
        m_toggled = 2;
        emit Pushed();
    }
}

void UIButton::Release(void)
{
    if (m_checkbox)
    {
        m_toggled--;
        if (m_toggled > 0)
            return;
    }

    m_toggled = 0;

    if (!m_focusable)
        LOG(VB_GENERAL, LOG_WARNING, "Releasing button that isn't focusable");

    bool signal = m_pushed;
    m_pushed = false;
    if (signal)
    {
        emit Released();

        if (m_actionReceiver && m_action)
        {
            TorcEvent *event = new TorcEvent(m_action, m_actionData);
            qApp->postEvent(m_actionReceiver, event);
        }
    }
}

void UIButton::SetFocusable(bool Focusable)
{
    if (Focusable)
    {
        if (!m_focusable)
            m_parent->IncreaseFocusableChildCount();
    }
    else
    {
        if (m_focusable)
            m_parent->DecreaseFocusableChildCount();

        if (m_parent && m_parent->GetLastChildWithFocus() == this)
            m_parent->SetLastChildWithFocus(NULL);

        if (GetFocusWidget() == this)
        {
            Deselect();

            // TODO get index from parent
            AutoSelectFocusWidget(0);
        }
    }

    m_focusable = Focusable;
}

static class UIButtonFactory : public WidgetFactory
{
    void RegisterConstructor(QScriptEngine *Engine)
    {
        if (!Engine)
            return;

        QScriptValue constructor = Engine->newFunction(UIButtonConstructor);
        Engine->globalObject().setProperty("UIButton", constructor);
    }

    UIWidget* Create(const QString &Type, UIWidget *Root, UIWidget *Parent,
                     const QString &Name, int Flags)
    {
        if (Type == "button")
            return new UIButton(Root, Parent, Name, Flags);
        return NULL;
    }

} UIButtonFactory;
