/* Class UIAnimation
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
#include <QMetaProperty>
#include <QDomNode>
#include <QPauseAnimation>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>

// Torc
#include "torclogging.h"
#include "uiwidget.h"
#include "uianimation.h"

int UIAnimation::kUIAnimationType = UIWidget::RegisterWidgetType();

static QScriptValue UIAnimationConstructor(QScriptContext *context, QScriptEngine *engine)
{
    return WidgetConstructor<UIAnimation>(context, engine);
}

UIAnimation::UIAnimation(UIWidget *Root, UIWidget *Parent, const QString &Name, int Flags)
  : UIWidget(Root, Parent, Name, Flags),
    m_animation(new QParallelAnimationGroup(this))
{
    m_type = kUIAnimationType;

    if (m_parent)
        connect(m_parent, SIGNAL(Hidden()), this, SLOT(Stop()));
}

UIAnimation::~UIAnimation()
{
    if (m_animation)
    {
        m_animation->stop();
        m_animation->clear();
    }

    delete m_animation;
    m_animation = NULL;
}

void UIAnimation::Start(void)
{
    if (m_animation && m_animation->animationCount() && !m_template)
        m_animation->start();
    emit Started();
}

void UIAnimation::Stop(void)
{
    if (m_animation)
        m_animation->stop();
    emit Finished();
}

bool UIAnimation::Finalise(void)
{
    if (m_template)
        return true;

    if (m_animation->animationCount())
        connect(m_animation, SIGNAL(finished()), this, SLOT(Stop()));

    return UIWidget::Finalise();
}

bool UIAnimation::InitialisePriv(QDomElement *Element)
{
    if (!Element)
        return false;

    if (Element->tagName() == "loopcount")
    {
        m_animation->setLoopCount(GetText(Element).toInt());
        return true;
    }
    else if (ParseAnimation(Element, m_animation))
    {
        return true;
    }

    return false;
}

void UIAnimation::CopyFrom(UIWidget *Other)
{
    UIAnimation *animation = dynamic_cast<UIAnimation*>(Other);

    if (!animation)
    {
        LOG(LOG_INFO, LOG_ERR, "Failed to cast widget to UIAnimation.");
        return;
    }

    if (m_animation && animation->m_animation->animationCount())
    {
        for (int i = 0; i < animation->m_animation->animationCount(); i++)
            CopyAnimation(m_animation, animation->m_animation->animationAt(i));
    }

    UIWidget::CopyFrom(Other);
}

void UIAnimation::CreateCopy(UIWidget* Parent)
{
    UIAnimation* animation = new UIAnimation(m_rootParent, Parent,
                                             GetDerivedWidgetName(Parent->objectName()),
                                             WidgetFlagNone);
    animation->CopyFrom(this);
}

void UIAnimation::CopyAnimation(QAbstractAnimation *Parent, QAbstractAnimation *Other)
{
    QPropertyAnimation *property = dynamic_cast<QPropertyAnimation*>(Other);
    if (property)
    {
        QByteArray type = property->propertyName();
        QPropertyAnimation *animation = new QPropertyAnimation((QObject*)m_parent, type, Parent);
        animation->setEndValue(property->endValue());
        animation->setLoopCount(property->loopCount());
        animation->setDuration(property->duration());
        animation->setEasingCurve(property->easingCurve());
        return;
    }

    QPauseAnimation *pause = dynamic_cast<QPauseAnimation*>(Other);
    if (pause)
    {
        QPauseAnimation *animation = new QPauseAnimation(pause->duration(), Parent);
        animation->setLoopCount(pause->loopCount());
        return;
    }

    QParallelAnimationGroup *parallel = dynamic_cast<QParallelAnimationGroup*>(Other);
    if (parallel && parallel->animationCount())
    {
        QAnimationGroup *animation = new QParallelAnimationGroup(Parent);
        animation->setLoopCount(parallel->loopCount());
        for (int i = 0; i < parallel->animationCount(); i++)
            CopyAnimation(animation, parallel->animationAt(i));
        return;
    }

    QSequentialAnimationGroup *sequential = dynamic_cast<QSequentialAnimationGroup*>(Other);
    if (sequential && sequential->animationCount())
    {
        QAnimationGroup *animation = new QSequentialAnimationGroup(Parent);
        animation->setLoopCount(sequential->loopCount());
        for (int i = 0; i < sequential->animationCount(); i++)
            CopyAnimation(animation, sequential->animationAt(i));
        return;
    }
}

bool UIAnimation::ParseAnimation(QDomElement *Element, QAbstractAnimation *Parent)
{
    if (!Element)
        return false;

    int count    = Element->attribute("loopcount", "1").toInt();
    int duration = Element->attribute("duration", "250").toInt();

    if (Element->tagName() == "pause")
    {
        QPauseAnimation *pause = new QPauseAnimation(duration, Parent);
        pause->setLoopCount(count);
        return true;
    }
    else if (Element->tagName() == "group")
    {
        QAnimationGroup *group = NULL;
        QString type = Element->attribute("type");

        if (type == "parallel")
            group = new QParallelAnimationGroup(Parent);
        else if (type == "sequential")
            group = new QSequentialAnimationGroup(Parent);

        if (group)
        {
            group->setLoopCount(count);
            for (QDomNode node = Element->firstChild(); !node.isNull(); node = node.nextSibling())
            {
                QDomElement element = node.toElement();
                if (element.isNull())
                    continue;

                ParseAnimation(&element, group);
            }

            if (!group->animationCount())
                LOG(VB_GENERAL, LOG_WARNING, "Animation group has no elements.");
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Unknown animation group type '%1'").arg(type));
        }
        return true;
    }
    else if (Element->tagName() == "element")
    {
        QByteArray property = Element->attribute("property").toLatin1();
        int index = m_parent->metaObject()->indexOfProperty(property);

        if (index < 0)
        {
            LOG(VB_GENERAL, LOG_WARNING, QString("Widget '%1' has no property named '%2'")
                .arg(m_parent->objectName()).arg(Element->attribute("property")));
            return true;
        }

        int propertytype = m_parent->metaObject()->property(index).type();

        bool ok = false;
        QPropertyAnimation *animation = NULL;
        QVariant endvalue;

        QString end = Element->attribute("endvalue");
        int curve = GetEasingCurve(Element->attribute("easingcurve"));

        switch (propertytype)
        {
            case QMetaType::Double:
            {
                endvalue = end.toFloat(&ok);
                if (!ok)
                    endvalue = QVariant();
                break;
            }
            case QMetaType::QRectF:
            {
                QRectF rect = m_parent->GetRect(end);
                if (rect.isValid())
                    endvalue = rect;
                break;
            }
            case QMetaType::QPointF:
            {
                endvalue = m_parent->GetPointF(end);
                break;
            }
            case QMetaType::QSizeF:
            {
                QSizeF size = m_parent->GetSizeF(end);
                if (size.isValid())
                    endvalue = size;
                break;
            }
            case QMetaType::Int:
            {
                endvalue = end.toInt(&ok);
                if (!ok)
                    endvalue = QVariant();
                break;
            }
            case QMetaType::UInt:
            {
                endvalue = end.toUInt(&ok);
                if (!ok)
                    endvalue = QVariant();
                break;
            }
            case QMetaType::ULongLong:
            {
                endvalue = end.toULongLong(&ok);
                if (!ok)
                    endvalue = QVariant();
                break;
            }
            default:
            {
                endvalue = end.toFloat(&ok);
                if (!ok)
                    endvalue = QVariant();
            }
        }

        if (endvalue.isValid())
            animation = new QPropertyAnimation((QObject*)m_parent, property, Parent);

        if (animation)
        {
            animation->setEndValue(endvalue);
            animation->setLoopCount(count);
            animation->setDuration(duration);
            animation->setEasingCurve((QEasingCurve::Type) curve);
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to create animation for variant type %1.")
                .arg(propertytype));
        }
        return true;
    }

    return false;
}

int UIAnimation::GetEasingCurve(const QString& EasingCurve)
{
    QString curve = EasingCurve.trimmed().toLower();

    if (curve      == "linear")       return QEasingCurve::Linear;
    else if (curve == "inquad")       return QEasingCurve::InQuad;
    else if (curve == "outquad")      return QEasingCurve::OutQuad;
    else if (curve == "inoutquad")    return QEasingCurve::InOutQuad;
    else if (curve == "outinquad")    return QEasingCurve::OutInQuad;
    else if (curve == "incubic")      return QEasingCurve::InCubic;
    else if (curve == "outcubic")     return QEasingCurve::OutCubic;
    else if (curve == "inoutcubic")   return QEasingCurve::InOutCubic;
    else if (curve == "outincubic")   return QEasingCurve::OutInCubic;
    else if (curve == "inquart")      return QEasingCurve::InQuart;
    else if (curve == "outquart")     return QEasingCurve::OutQuart;
    else if (curve == "inoutquart")   return QEasingCurve::InOutQuart;
    else if (curve == "outinquart")   return QEasingCurve::OutInQuart;
    else if (curve == "inquint")      return QEasingCurve::InQuint;
    else if (curve == "outquint")     return QEasingCurve::OutQuint;
    else if (curve == "inoutquint")   return QEasingCurve::InOutQuint;
    else if (curve == "outinquint")   return QEasingCurve::OutInQuint;
    else if (curve == "insine")       return QEasingCurve::InSine;
    else if (curve == "outsine")      return QEasingCurve::OutSine;
    else if (curve == "inoutsine")    return QEasingCurve::InOutSine;
    else if (curve == "outinsine")    return QEasingCurve::OutInSine;
    else if (curve == "inexpo")       return QEasingCurve::InExpo;
    else if (curve == "outexpo")      return QEasingCurve::OutExpo;
    else if (curve == "inoutexpo")    return QEasingCurve::InOutExpo;
    else if (curve == "outinexpo")    return QEasingCurve::OutInExpo;
    else if (curve == "incirc")       return QEasingCurve::InCirc;
    else if (curve == "outcirc")      return QEasingCurve::OutCirc;
    else if (curve == "onoutcirc")    return QEasingCurve::InOutCirc;
    else if (curve == "outincirc")    return QEasingCurve::OutInCirc;
    else if (curve == "inelastic")    return QEasingCurve::InElastic;
    else if (curve == "outelastic")   return QEasingCurve::OutElastic;
    else if (curve == "inoutelastic") return QEasingCurve::InOutElastic;
    else if (curve == "outinelastic") return QEasingCurve::OutInElastic;
    else if (curve == "inback")       return QEasingCurve::InBack;
    else if (curve == "outback")      return QEasingCurve::OutBack;
    else if (curve == "inoutback")    return QEasingCurve::InOutBack;
    else if (curve == "outinback")    return QEasingCurve::OutInBack;
    else if (curve == "inbounce")     return QEasingCurve::InBounce;
    else if (curve == "outbounce")    return QEasingCurve::OutBounce;
    else if (curve == "inoutbounce")  return QEasingCurve::InOutBounce;
    else if (curve == "outinbounce")  return QEasingCurve::OutInBounce;
    else if (curve == "inburve")      return QEasingCurve::InCurve;
    else if (curve == "outburve")     return QEasingCurve::OutCurve;
    else if (curve == "sinecurve")    return QEasingCurve::SineCurve;
    else if (curve == "cosinecurve")  return QEasingCurve::CosineCurve;

    return QEasingCurve::Linear;
}

static class UIAnimationFactory : public WidgetFactory
{
    void RegisterConstructor(QScriptEngine *Engine)
    {
        if (!Engine)
            return;

        QScriptValue constructor = Engine->newFunction(UIAnimationConstructor);
        Engine->globalObject().setProperty("UIAnimation", constructor);
    }

    UIWidget* Create(const QString &Type, UIWidget *Root, UIWidget *Parent,
                     const QString &Name, int Flags)
    {
        if (Type == "animation")
            return new UIAnimation(Root, Parent, Name, Flags);
        return NULL;
    }

} UIAnimationFactory;

