/* Class TenfootClock
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
#include <QtScript>
#include <QDomNode>

// Torc
#include "uitext.h"
#include "tenfootclock.h"

int TenfootClock::kTenfootClockType = UIWidget::RegisterWidgetType();

static QScriptValue TenfootClockConstructor(QScriptContext *context, QScriptEngine *engine)
{
    return WidgetConstructor<TenfootClock>(context, engine);
}

TenfootClock::TenfootClock(UIWidget *Root, UIWidget *Parent, const QString &Name, int Flags)
  : UIWidget(Root, Parent, Name, Flags),
    m_smooth(false),
    m_nextUpdate(0),
    m_baseTickcount(0),
    m_secondHand(NULL),
    m_minuteHand(NULL),
    m_hourHand(NULL),
    m_text(NULL),
    smooth(false)
{
    m_type = kTenfootClockType;
}

TenfootClock::~TenfootClock()
{
}

bool TenfootClock::GetSmooth(void)
{
    return m_smooth;
}

void TenfootClock::SetSmooth(bool Smooth)
{
    m_smooth = Smooth;
}

bool TenfootClock::InitialisePriv(QDomElement *Element)
{
    if (!Element)
        return false;

    if (Element->tagName() == "smooth")
    {
        m_smooth = GetBool(Element);
        smooth = m_smooth;
        return true;
    }

    return false;
}

void TenfootClock::CopyFrom(UIWidget *Other)
{
    TenfootClock *clock = dynamic_cast<TenfootClock*>(Other);
    if (!clock)
        return;

    SetSmooth(clock->GetSmooth());

    UIWidget::CopyFrom(Other);
}

void TenfootClock::CreateCopy(UIWidget* Parent)
{
    TenfootClock* clock = new TenfootClock(m_rootParent, Parent,
                                           GetDerivedWidgetName(Parent->objectName()),
                                           WidgetFlagNone);
    clock->CopyFrom(this);
}

bool TenfootClock::Refresh(quint64 TimeNow)
{
    if (!m_enabled || m_template)
        return false;

    if (m_baseTickcount < 1)
    {
        m_baseTickcount = TimeNow;
        m_baseTime      = QDateTime::currentDateTime();
        m_nextUpdate    = TimeNow - 1;
    }

    if (!m_smooth || (m_smooth && !m_secondHand))
    {
        if (TimeNow < m_nextUpdate)
            return false;
        m_nextUpdate += 1000000;
    }

    m_displayTime = m_baseTime.addMSecs((TimeNow - m_baseTickcount) / 1000);

    // TODO time format
    if (m_text)
        m_text->SetText(m_displayTime.toString("hh:mm:ss"));

    return UIWidget::Refresh(TimeNow);
}

bool TenfootClock::DrawSelf(UIWindow *Window, qreal XOffset, qreal YOffset)
{
    if (!Window)
        return false;

    QTime time = m_displayTime.time();

    qreal seconds = time.second() + (m_smooth ? (time.msec() / 1000.0) : 0.0);
    qreal minutes = time.minute() + seconds / 60.0;
    qreal hours   = time.hour() + minutes / 60.0;

    if (m_secondHand)
        m_secondHand->SetRotation(seconds * 6.0);

    if (m_minuteHand)
        m_minuteHand->SetRotation(minutes  * 6.0);

    if (m_hourHand)
        m_hourHand->SetRotation(hours * 30.0);

    return true;
}

bool TenfootClock::Finalise(void)
{
    if (m_template)
        return true;

    if (!m_secondHand)
        m_secondHand = FindChildByName(objectName() + "_secondhand");

    if (!m_minuteHand)
        m_minuteHand = FindChildByName(objectName() + "_minutehand");

    if (!m_hourHand)
        m_hourHand = FindChildByName(objectName() + "_hourhand");

    if (!m_text)
    {
        UIWidget *text = FindChildByName(objectName() + "_text");
        if (text && text->Type() == UIText::kUITextType)
            m_text = dynamic_cast<UIText*>(text);
    }

    return UIWidget::Finalise();
}

static class TenfootClockFactory : public WidgetFactory
{
    void RegisterConstructor(QScriptEngine *Engine)
    {
        if (!Engine)
            return;

        QScriptValue constructor = Engine->newFunction(TenfootClockConstructor);
        Engine->globalObject().setProperty("TenfootClock", constructor);
    }

    UIWidget* Create(const QString &Type, UIWidget *Root, UIWidget *Parent,
                     const QString &Name, int Flags)
    {
        if (Type == "tenfootclock")
            return new TenfootClock(Root, Parent, Name, Flags);
        return NULL;
    }

} TenfootClockFactory;

