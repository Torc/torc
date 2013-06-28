/* Class UIMessenger
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
#include <QTimerEvent>
#include <QAtomicInt>

// Torc
#include "torclocalcontext.h"
#include "uigroup.h"
#include "uitext.h"
#include "uimessenger.h"

#define MESSAGE_TYPE_ERROR    QString("MESSAGETYPEERROR")
#define MESSAGE_TYPE_WARNING  QString("MESSAGETYPEWARNING")
#define MESSAGE_TYPE_CRITICAL QString("MESSAGETYPECRITICAL")
#define MESSAGE_TYPE_EXTERNAL QString("MESSAGETYPEEXTERNAL")
#define MESSAGE_TYPE_INTERNAL QString("MESSAGETYPEINTERNAL")
#define MESSAGE_HEADER        QString("MESSAGEHEADER")
#define MESSAGE_BODY          QString("MESSAGEBODY")

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

    m_messageTemplate = FindChildByName("template");

    UIWidget *group = FindChildByName("group");
    if (group && group->Type() == UIGroup::kUIGroupType)
        m_messageGroup = dynamic_cast<UIGroup*>(group);

    if (!m_messageTemplate || !m_messageGroup)
        return false;

    return UIWidget::Finalise();
}

UIWidget* UIMessenger::CreateCopy(UIWidget *Parent, const QString &Newname)
{
    bool isnew = !Newname.isEmpty();
    UIMessenger* messenger = new UIMessenger(m_rootParent, Parent,
                                             isnew ? Newname : GetDerivedWidgetName(Parent->objectName()),
                                             (!isnew && IsTemplate()) ? WidgetFlagTemplate : WidgetFlagNone);
    messenger->CopyFrom(this);
    return messenger;
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
    if (torcevent && torcevent->GetEvent() == Torc::Message)
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

        QString name = objectName() + "_" + uuid;
        UIWidget *message = m_messageTemplate->CreateCopy(m_messageGroup, name);

        // make sure the widget is deleted when it's been displayed
        UIWidget *deactivated = message->FindChildByName("Deactivated");
        if (deactivated)
            message->Connect(name + "_Deactivated", "Finished", name, "Close");
        else
            message->Connect("", "Deactivated", "", "Close");

        QString widget;
        switch (type)
        {
            case Torc::GenericError:    widget = MESSAGE_TYPE_ERROR;    break;
            case Torc::GenericWarning:  widget = MESSAGE_TYPE_WARNING;  break;
            case Torc::CriticalError:   widget = MESSAGE_TYPE_CRITICAL; break;
            case Torc::ExternalMessage: widget = MESSAGE_TYPE_EXTERNAL; break;
            default:                    widget = MESSAGE_TYPE_INTERNAL; break;
        }

        UIWidget *typewidget = message->FindChildByName(widget, true);
        if (!typewidget && widget != MESSAGE_TYPE_INTERNAL)
            typewidget = FindChildByName(MESSAGE_TYPE_INTERNAL);

        if (typewidget)
            typewidget->Show();

        if (!header.isEmpty())
        {
            UIWidget *headerwidget = FindChildByName(MESSAGE_HEADER, true);
            if (headerwidget && headerwidget->Type() == UIText::kUITextType)
                dynamic_cast<UIText*>(headerwidget)->SetText(header);
        }

        if (!body.isEmpty())
        {
            UIWidget *bodywidget   = FindChildByName(MESSAGE_BODY, true);
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
