/* Class UIMessenger
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
#include <QTimerEvent>
#include <QAtomicInt>

// Torc
#include "torclocalcontext.h"
#include "uigroup.h"
#include "uitext.h"
#include "uimessenger.h"

int UIMessenger::kUIMessengerType = UIWidget::RegisterWidgetType();

static QScriptValue UIMessengerConstructor(QScriptContext *context, QScriptEngine *engine)
{
    return WidgetConstructor<UIMessenger>(context, engine);
}

UIMessenger::UIMessenger(UIWidget *Root, UIWidget *Parent, const QString &Name, int Flags)
  : UIWidget(Root, Parent, Name, Flags),
    m_messageTemplate(NULL),
    m_messageGroup(NULL)
{
    m_type = kUIMessengerType;
    gLocalContext->AddObserver(this);
}

UIMessenger::~UIMessenger()
{
    gLocalContext->RemoveObserver(this);
}

bool UIMessenger::Finalise(void)
{
    if (m_template)
        return true;

    m_messageTemplate = FindChildByName(objectName() + "_template");

    UIWidget *group = FindChildByName(objectName() + "_group");
    if (group && group->Type() == UIGroup::kUIGroupType)
        m_messageGroup = dynamic_cast<UIGroup*>(group);

    if (!m_messageTemplate || !m_messageGroup)
        return false;

    return UIWidget::Finalise();
}

void UIMessenger::CreateCopy(UIWidget *Parent)
{
    UIMessenger* messenger = new UIMessenger(m_rootParent, Parent,
                                             GetDerivedWidgetName(Parent->objectName()),
                                             WidgetFlagNone);
    messenger->CopyFrom(this);
}

bool UIMessenger::event(QEvent *Event)
{
    if (Event->type() == QEvent::Timer)
    {
        QTimerEvent *event = dynamic_cast<QTimerEvent*>(Event);
        if (event)
        {
            int id = event->timerId();
            killTimer(id);
            QMap<int,UIWidget*>::iterator it = m_timeouts.find(id);
            if (it != m_timeouts.end())
            {
                it.value()->Deactivate();
                m_timeouts.remove(id);
            }
        }

        return true;
    }

    if (Event->type() != TorcEvent::TorcEventType ||
        !m_messageTemplate || !m_messageGroup)
    {
        return false;
    }

    TorcEvent* torcevent = dynamic_cast<TorcEvent*>(Event);
    if (torcevent && torcevent->Event() == Torc::Message)
    {
        QVariantMap data = torcevent->Data();

        int destination = data.value("destination").toInt();
        int timeout     = data.value("timeout").toInt();

        // we only handle local events with a timeout (otherwise needs a popup)
        if ((!(destination & Torc::Local)) || timeout < 1)
            return false;

        int type       = data.value("type").toInt();
        QString header = data.value("header").toString();
        QString body   = data.value("body").toString();
        QString uuid   = data.value("uuid").toString();

        if (header.isEmpty() && body.isEmpty())
            return false;

        QString name = QString("%1_%2").arg(objectName()).arg(uuid);
        UIWidget *message = new UIWidget(m_rootParent, m_messageGroup,
                                         name, WidgetFlagNone);
        message->CopyFrom(m_messageTemplate);

        // make sure the widget is deleted when it's been displayed
        UIWidget *deactivated = message->FindChildByName(name + "_Deactivated");
        if (deactivated)
            message->Connect(name + "_Deactivated", "Finished", name, "Close");
        else
            message->Connect("", "Deactivated", "", "Close");

        QString widget;
        switch (type)
        {
            case Torc::GenericError:    widget = "_error";    break;
            case Torc::GenericWarning:  widget = "_warning";  break;
            case Torc::CriticalError:   widget = "_critical"; break;
            case Torc::ExternalMessage: widget = "_external"; break;
            default:                    widget = "_internal"; break;
        }

        UIWidget *typewidget = FindWidget(message->objectName() + widget);
        if (!typewidget && widget != "_internal")
            typewidget = FindWidget(message->objectName() + "_internal");

        if (typewidget)
            typewidget->Show();

        if (!header.isEmpty())
        {
            UIWidget *headerwidget = FindWidget(message->objectName() + "_header");
            if (headerwidget && headerwidget->Type() == UIText::kUITextType)
                dynamic_cast<UIText*>(headerwidget)->SetText(header);
        }

        if (!body.isEmpty())
        {
            UIWidget *bodywidget   = FindWidget(message->objectName() + "_body");
            if (bodywidget && bodywidget->Type() == UIText::kUITextType)
                dynamic_cast<UIText*>(bodywidget)->SetText(body);
        }

        m_timeouts.insert(startTimer(timeout), message);
        message->Activate();
    }

    return false;
}

static class UIMessengerFactory : public WidgetFactory
{
    void RegisterConstructor(QScriptEngine *Engine)
    {
        if (!Engine)
            return;

        QScriptValue constructor = Engine->newFunction(UIMessengerConstructor);
        Engine->globalObject().setProperty("UIMessenger", constructor);
    }

    UIWidget* Create(const QString &Type, UIWidget *Root, UIWidget *Parent,
                     const QString &Name, int Flags)
    {
        if (Type == "messenger")
            return new UIMessenger(Root, Parent, Name, Flags);
        return NULL;
    }

} UIMessengerFactory;
