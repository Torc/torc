/* Class UIGroup
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
#include "uiwindow.h"
#include "uieffect.h"
#include "uianimation.h"
#include "uigroup.h"

#define NOT_VISIBLE(WIDGET) (WIDGET->m_template || WIDGET->m_effect->m_decoration || WIDGET->m_effect->m_detached || !WIDGET->m_visible || WIDGET->m_type == UIAnimation::kUIAnimationType)
#define IS_DETACHED(WIDGET) (WIDGET->m_effect->m_decoration || WIDGET->m_effect->m_detached)

int UIGroup::kUIGroupType = UIWidget::RegisterWidgetType();

static QScriptValue UIGroupConstructor(QScriptContext *context, QScriptEngine *engine)
{
    return WidgetConstructor<UIGroup>(context, engine);
}

UIGroup::UIGroup(UIWidget *Root, UIWidget *Parent, const QString &Name, int Flags)
  : UIWidget(Root, Parent, Name, Flags),
    m_groupType(None),
    m_wrapSelection(false),
    m_alignment(Qt::AlignLeft | Qt::AlignTop),
    m_fixedWidth(0.0),
    m_fixedHeight(0.0),
    m_spacingX(0.0),
    m_spacingY(0.0),
    m_easingCurve(NULL),
    m_speed(0.0),
    m_duration(-1.0),
    m_startTime(0),
    m_start(0.0),
    m_finish(0.0),
    m_lastOffset(0.0),
    m_duration2(-1.0),
    m_startTime2(0),
    m_start2(0.0),
    m_finish2(0.0),
    m_lastOffset2(0.0),
    m_skip(false),
    grouptype(None),
    wrapSelection(false),
    alignment(Qt::AlignLeft | Qt::AlignTop),
    fixedwidth(0.0),
    fixedheight(0.0),
    spacingX(0.0),
    spacingY(0.0)
{
    m_type = kUIGroupType;
}

UIGroup::~UIGroup()
{
    delete m_easingCurve;
    m_easingCurve = NULL;
}

bool UIGroup::InitialisePriv(QDomElement *Element)
{
    if (!Element)
        return false;

    bool ok = false;

    if (Element->tagName() == "scroll")
    {
        QString wraps = Element->attribute("wrapselection");
        QString curve = Element->attribute("easingcurve");
        QString speed = Element->attribute("speed"); // pixels per second

        if (!wraps.isEmpty())
            m_wrapSelection = GetBool(wraps);
        wrapSelection = m_wrapSelection;

        if (!curve.isEmpty())
            SetAnimationCurve(UIAnimation::GetEasingCurve(curve));

        if (!speed.isEmpty())
        {
            qreal speedf = speed.toFloat(&ok);
            SetAnimationSpeed(ok ? speedf : 1000.0);
        }
        else
        {
            SetAnimationSpeed(1000.0);
        }

        return true;
    }
    else if (Element->tagName() == "layout")
    {
        QString types      = Element->attribute("type");
        QString alignments = Element->attribute("alignment");
        QString spacingx   = Element->attribute("spacingwidth");
        QString spacingy   = Element->attribute("spacingheight");
        QString width      = Element->attribute("fixedwidth");
        QString height     = Element->attribute("fixedheight");
        QString range      = Element->attribute("selectrange");

        if (types      == "horizontal")
            m_groupType = Horizontal;
        else if (types == "vertical")
            m_groupType = Vertical;
        else if (types == "grid")
            m_groupType = Grid;
        grouptype = m_groupType;

        if (!alignments.isEmpty())
            m_alignment = (Qt::Alignment)GetAlignment(alignments);
        alignment = m_alignment;

        if (!spacingx.isEmpty())
        {
            qreal spacingf = spacingx.toFloat(&ok);
            if (ok)
                SetSpacingX(spacingf);
        }

        if (!spacingy.isEmpty())
        {
            qreal spacingf = spacingy.toFloat(&ok);
            if (ok)
                SetSpacingY(spacingf);
        }

        spacingX = m_spacingX;
        spacingY = m_spacingY;

        if (!width.isEmpty())
        {
            if (m_groupType != Vertical)
            {
                qreal widthf = width.toFloat(&ok);
                if (ok)
                    SetFixedWidth(widthf);
            }
            else
            {
                LOG(VB_GENERAL, LOG_WARNING, "Cannot set fixed width for vertical group.");
            }
        }

        if (!height.isEmpty())
        {
            if (m_groupType != Horizontal)
            {
                qreal heightf = height.toFloat(&ok);
                if (ok)
                    SetFixedHeight(heightf);
            }
            else
            {
                LOG(VB_GENERAL, LOG_WARNING, "Cannot set fixed height for horizontal group.");
            }
        }

        SetSelectionRange(range.isEmpty() ? QRectF() : UIWidget::GetRect(range));

        if (m_groupType == Grid && (m_fixedWidth < 1.0 || m_fixedHeight < 1.0))
        {
            LOG(VB_GENERAL, LOG_WARNING, "Grid requires a fixed width and fixed height.");
            return false;
        }

        return true;
    }

    return false;
}

UIWidget* UIGroup::FirstFocusWidget(void)
{
    foreach (UIWidget* child, m_children)
        if (child->IsFocusable())
            return child;

    return NULL;
}

UIWidget* UIGroup::LastFocusWidget(void)
{
    QList<UIWidget*>::const_iterator it = m_children.end();

    for (--it; it != m_children.begin(); --it)
        if ((*it)->IsFocusable())
            return *it;

    return NULL;
}

UIWidget* UIGroup::GetFocusableChild(int Index)
{
    int index = qBound(0, Index, m_children.size() - 1);

    for (int i = 0, j = 0; i < m_children.size(); i++)
    {
        if (!m_children.at(i)->IsTemplate() && !m_children.at(i)->IsDecoration())
        {
            if (index == j)
            {
                if (m_children.at(i)->IsFocusable())
                    return m_children.at(i);
                return NULL;
            }
            j++;
        }
    }

    return NULL;
}

void UIGroup::GetGridPosition(UIWidget* Widget, int &Row, int &Column, int &TotalRows, int &TotalColumns)
{
    if (m_groupType != Grid)
        return;

    int index = m_children.indexOf(Widget);

    // calculate the index of the current widget excluding decorations etc
    int visiblecount = 0;
    int visibleindex = 0;
    for (int i = 0; i < (int)m_children.size(); ++i)
    {
        if (NOT_VISIBLE(m_children.at(i)))
            continue;

        visiblecount++;
        if (i < index)
            visibleindex++;
    }

    TotalColumns = (int)(m_scaledRect.size().width()  / m_fixedWidth);
    if (TotalColumns < 1)
        return;

    if (TotalColumns > visiblecount)
        TotalColumns = visiblecount;

    TotalRows = ceil((qreal)visiblecount / TotalColumns);
    if (TotalRows < 1)
        return;

    Row = visibleindex / TotalColumns;
    Column = visibleindex - (Row * TotalColumns);
}

bool UIGroup::HandleAction(int Action)
{
    // reset scrolling
    m_startTime = 0;

    // save the unadjusted action
    int action = Action;

    // reverse the direction for bottom to top
    if ((m_alignment & Qt::AlignBottom) &&
        (m_groupType == Grid || m_groupType == Vertical))
    {
        if (Action == Torc::Up)
            action = Torc::Down;
        else if (Action == Torc::Down)
            action = Torc::Up;
    }

    // reverse the direction for right to left
    if ((m_alignment & Qt::AlignRight) &&
        (m_groupType == Grid || m_groupType == Horizontal))
    {
        if (Action == Torc::Left)
            action = Torc::Right;
        else if (Action == Torc::Right)
            action = Torc::Left;
    }

    // disable wrapping if it breaks menu navigation
    bool wrapvertical   = m_wrapSelection;
    bool wraphorizontal = m_wrapSelection;

    if (m_wrapSelection && m_parent && m_parent->Type() == kUIGroupType)
    {
        UIGroup* widget = static_cast<UIGroup*>(m_parent);
        if (widget)
        {
            int type = widget->GetGroupType();
            if (type == Vertical || type == Grid)
                wrapvertical = false;
            else if (type == Horizontal || type == Grid)
                wraphorizontal = false;
        }
    }

    if (m_groupType == Vertical)
    {
        // pass left/right to parent group if it exists
        if (action == Torc::Left || action == Torc::Right)
        {
            if (m_parent && m_parent->IsFocusable())
                return m_parent->HandleAction(action);
            return false;
        }

        // handle end of list
        if (action == Torc::Up)
        {
            if (GetFocusWidget() == FirstFocusWidget())
            {
                if (!wrapvertical)
                {
                    // pass to parent group or ignore
                    if (m_parent && m_parent->IsFocusable())
                        return m_parent->HandleAction(Action);
                    return false;
                }
                else
                {
                    // don't animate from one end of the list to the other...
                    m_skip = true;
                }
            }
        }
        else if (action == Torc::Down)
        {
            if (GetFocusWidget() == LastFocusWidget())
            {
                if (!wrapvertical)
                {
                    if (m_parent && m_parent->IsFocusable())
                        return m_parent->HandleAction(Action);
                    return false;
                }
                else
                {
                    m_skip = true;
                }
            }
        }
    }
    else if (m_groupType == Horizontal)
    {
        // pass up/down to parent group if it exists
        if (action == Torc::Up || action == Torc::Down)
        {
            if (m_parent && m_parent->IsFocusable())
                return m_parent->HandleAction(action);
            return false;
        }

        // handle end of list
        if (action == Torc::Left)
        {
            if (GetFocusWidget() == FirstFocusWidget())
            {
                if (!wraphorizontal)
                {
                    // pass to parent group or ignore
                    if (m_parent && m_parent->IsFocusable())
                        return m_parent->HandleAction(Action);
                    return false;
                }
                else
                {
                    // don't animate from one end of the list to the other...
                    m_skip = true;
                }
            }
        }
        else if (action == Torc::Right)
        {
            if (GetFocusWidget() == LastFocusWidget())
            {
                if (!wraphorizontal)
                {
                    if (m_parent && m_parent->IsFocusable())
                        return m_parent->HandleAction(Action);
                    return false;
                }
                else
                {
                    m_skip = true;
                }
            }
        }
    }
    else if (m_groupType == Grid && m_fixedWidth >= 1.0 && m_fixedHeight >= 1.0)
    {
        // find the selected widget (if any)
        UIWidget* focuswidget = GetFocusWidget();
        bool found = m_children.contains(focuswidget);

        // check if focus widget is in nested group
        if (!found)
        {
            for (int i = 0; i < m_children.size(); ++i)
            {
                if (m_children.at(i)->HasChildWithFocus())
                {
                    focuswidget = m_children.at(i);
                    found = true;
                    break;
                }
            }
        }

        if (m_children.contains(focuswidget))
        {
            int row, col, rows, cols = 0;
            GetGridPosition(focuswidget, row, col, rows, cols);
            bool select = false;

            if (action == Torc::Left)
            {
                if (col == 0)
                {
                    if (!wraphorizontal)
                    {
                        // pass action to parent group
                        if (m_parent && m_parent->IsFocusable())
                            return m_parent->HandleAction(Action);
                        return false;
                    }
                    else
                    {
                        m_skip = true;
                    }
                }
                select = true;
            }
            else if (action == Torc::Right)
            {
                if (col == cols - 1)
                {
                    if (!wraphorizontal)
                    {
                        if (m_parent && m_parent->IsFocusable())
                            return m_parent->HandleAction(Action);
                        return false;
                    }
                    else
                    {
                        m_skip = true;
                    }
                }
                select = true;
            }
            else if (action == Torc::Up)
            {
                if (row == 0)
                {
                    if (!wrapvertical)
                    {
                        if (m_parent && m_parent->IsFocusable())
                            return m_parent->HandleAction(Action);
                        return false;
                    }
                    else
                    {
                        m_skip = true;
                    }
                }
                select = true;
            }
            else if (action == Torc::Down)
            {
                if (row == (rows - 1))
                {
                    if (!wrapvertical)
                    {
                        if (m_parent && m_parent->IsFocusable())
                            return m_parent->HandleAction(Action);
                        return false;
                    }
                    else
                    {
                        m_skip = true;
                    }
                }
                select = true;
            }

            if (select)
            {
                bool pass = false;
                UIWidget* child = SelectChildInGrid(action, row, col, rows, cols,
                                                    wrapvertical, wraphorizontal, pass);
                if (child)
                    child->Select();
                else if (pass && m_parent && m_parent->IsFocusable())
                    return m_parent->HandleAction(Action);

                return true;
            }
        }
    }

    return UIWidget::HandleAction(action);
}

void UIGroup::CopyFrom(UIWidget *Other)
{
    UIGroup *group = dynamic_cast<UIGroup*>(Other);

    if (!group)
    {
        LOG(LOG_INFO, LOG_ERR, "Failed to cast widget to UIGroup.");
        return;
    }

    SetType(group->GetGroupType());
    SetWrapSelection(group->GetWrapSelection());
    SetAlignment(group->GetGroupAlignment());
    // N.B. using setters here will re-apply scaling
    m_fixedWidth  = group->m_fixedWidth;
    m_fixedHeight = group->m_fixedHeight;
    m_spacingX    = group->m_spacingX;
    m_spacingY    = group->m_spacingY;
    m_selectionRange = group->m_selectionRange;
    m_speed = group->m_speed;
    if (group->m_easingCurve)
        SetAnimationCurve((int)group->m_easingCurve->type());

    grouptype   = group->grouptype;
    alignment   = group->alignment;
    fixedwidth  = group->fixedwidth;
    fixedheight = group->fixedheight;
    spacingX    = group->spacingX;
    spacingY    = group->spacingY;

    UIWidget::CopyFrom(Other);
}

UIWidget* UIGroup::CreateCopy(UIWidget* Parent, const QString &Newname)
{
    UIGroup* group = new UIGroup(m_rootParent, Parent,
                                 Newname.isEmpty() ? GetDerivedWidgetName(Parent->objectName()) : Newname,
                                 WidgetFlagNone);
    group->CopyFrom(this);
    return group;
}

int UIGroup::GetGroupType(void)
{
    return m_groupType;
}

bool UIGroup::GetWrapSelection(void)
{
    return m_wrapSelection;
}

int UIGroup::GetGroupAlignment(void)
{
    return m_alignment;
}

qreal UIGroup::GetFixedWidth(void)
{
    return m_fixedWidth;
}

qreal UIGroup::GetFixedHeight(void)
{
    return m_fixedHeight;
}

qreal UIGroup::GetSpacingX(void)
{
    return m_spacingX;
}

qreal UIGroup::GetSpacingY(void)
{
    return m_spacingY;
}

void UIGroup::SetType(int Type)
{
    m_groupType = (GroupType)Type;
}

void UIGroup::SetWrapSelection(bool Wrap)
{
    m_wrapSelection = Wrap;
}

void UIGroup::SetAlignment(int Alignment)
{
    m_alignment = (Qt::AlignmentFlag)Alignment;
}

void UIGroup::SetFixedWidth(qreal Width)
{
    m_fixedWidth = Width * m_rootParent->GetXScale();
}

void UIGroup::SetFixedHeight(qreal Height)
{
    m_fixedHeight = Height * m_rootParent->GetYScale();
}

void UIGroup::SetSpacingX(qreal Spacing)
{
    m_spacingX = Spacing * m_rootParent->GetXScale();
}

void UIGroup::SetSpacingY(qreal Spacing)
{
    m_spacingY = Spacing * m_rootParent->GetYScale();
}

void UIGroup::SetSelectionRange(const QRectF &Range)
{
    m_selectionRange = ScaleRect(Range);

    // TODO hook up to size changes
    QRectF bounding(QPointF(0.0, 0.0), m_scaledRect.size());

    if (!m_selectionRange.isValid())
        m_selectionRange = bounding;
    else if (!bounding.contains(m_selectionRange))
        m_selectionRange = bounding;
}

void UIGroup::SetSelectionRange(qreal Left, qreal Top, qreal Width, qreal Height)
{
    QRectF range(Left, Top, Width, Height);
    SetSelectionRange(range);
}

void UIGroup::SetAnimationCurve(int EasingCurve)
{
    delete m_easingCurve;

    if (EasingCurve > -1)
        m_easingCurve = new QEasingCurve((QEasingCurve::Type)EasingCurve);
}

void UIGroup::SetAnimationSpeed(qreal Speed)
{
    if (m_groupType == None)
        LOG(VB_GENERAL, LOG_WARNING, "Setting animation speed but group type not set. Set layout first?");

    Speed /= 1000000.0;

    if (!m_rootParent)
        m_speed = Speed;
    else if (m_groupType == Vertical)
        m_speed = Speed * m_rootParent->GetYScale();
    else
        m_speed = Speed * m_rootParent->GetXScale();
}

bool UIGroup::Draw(quint64 TimeNow, UIWindow* Window, qreal XOffset, qreal YOffset)
{
    if (m_groupType == None)
        return UIWidget::Draw(TimeNow, Window, XOffset, YOffset);

    if (!m_visible || !Window)
        return false;

    qreal xoffset = m_scaledRect.left() + (m_effect->m_detached ? 0.0 : XOffset);
    qreal yoffset = m_scaledRect.top()  + (m_effect->m_detached ? 0.0 : YOffset);

    // find the selected widget (if any)
    UIWidget* focuswidget = GetFocusWidget();
    int focusindex = m_children.indexOf(focuswidget);

    // check if focus widget is in nested group
    if (focusindex < 0)
    {
        for (int i = 0; i < m_children.size(); ++i)
        {
            if (m_children.at(i)->HasChildWithFocus())
            {
                focusindex = i;
                focuswidget = m_children.at(i);
                break;
            }
        }
    }

    // and finally check for last focused child - this ensures nested groups
    // are rendered correctly when the current item is 'off screen'
    if (focusindex < 0)
       focusindex = m_children.indexOf(m_lastChildWithFocus);

    // sanitise
    if (focusindex < 0)
        focusindex = 0;

    // effects
    Window->PushEffect(m_effect, &m_scaledRect);

    if (m_secondaryEffect)
    {
        if (m_clipping)
        {
            QRect clip(m_clipRect.translated(m_scaledRect.left(), m_scaledRect.top()));

            if (m_secondaryEffect->m_hReflecting)
                clip.moveTop(yoffset + m_secondaryEffect->m_hReflection + (yoffset + m_secondaryEffect->m_hReflection - clip.top() - clip.height()));
            else if (m_secondaryEffect->m_vReflecting)
                clip.moveLeft(xoffset + m_secondaryEffect->m_vReflection + (xoffset + m_secondaryEffect->m_vReflection - clip.left() - clip.width()));

            Window->PushClip(clip);
        }

        QRectF rect(0, 0, m_scaledRect.width(), m_scaledRect.height());
        Window->PushEffect(m_secondaryEffect, &rect);
        DrawGroup(TimeNow, Window, xoffset, yoffset, focusindex, focuswidget);
        Window->PopEffect();

        if (m_clipping)
            Window->PopClip();
    }

    if (m_clipping)
    {
        QRect clip(m_clipRect.translated(m_scaledRect.left(), m_scaledRect.top()));

        if (m_effect->m_hReflecting)
            clip.moveTop(yoffset + m_effect->m_hReflection + (yoffset + m_effect->m_hReflection - clip.top() - clip.height()));
        else if (m_effect->m_vReflecting)
            clip.moveLeft(xoffset + m_effect->m_vReflection + (xoffset + m_effect->m_vReflection - clip.left() - clip.width()));

        Window->PushClip(clip);
    }

    DrawGroup(TimeNow, Window, xoffset, yoffset, focusindex, focuswidget);

    // revert effects
    if (m_clipping)
        Window->PopClip();

    Window->PopEffect();

    return true;
}

void UIGroup::DrawGroup(quint64 TimeNow, UIWindow* Window, qreal XOffset, qreal YOffset,
                        int FocusIndex, UIWidget *FocusWidget)
{
    // selected widget to be drawn last
    QPointF toppos;
    UIWidget* topwidget = NULL;

    // draw decoration widgets (i.e. background)
    foreach (UIWidget* child, m_children)
        if (IS_DETACHED(child))
            child->Draw(TimeNow, Window, XOffset, YOffset);

    QPointF position(0.0, 0.0);
    bool vcenter     = m_alignment & Qt::AlignVCenter || m_alignment & Qt::AlignCenter;
    bool hcenter     = m_alignment & Qt::AlignHCenter || m_alignment & Qt::AlignCenter;
    bool toptobottom = m_alignment & Qt::AlignTop;
    bool bottomtotop = m_alignment & Qt::AlignBottom;
    bool lefttoright = m_alignment & Qt::AlignLeft;
    bool righttoleft = m_alignment & Qt::AlignRight;

    if (m_groupType == Horizontal && (lefttoright || righttoleft))
    {
        qreal leftbar  = m_selectionRange.left();
        qreal rightbar = m_scaledRect.width() - m_selectionRange.right();
        qreal height   = m_scaledRect.height();
        qreal limit    = m_scaledRect.left() + XOffset;

        if (lefttoright)
            limit += m_scaledRect.width();
        else
            position += QPointF(m_scaledRect.width(), 0.0);

        // calculate any offset
        qreal leftoffset = 0.0;
        qreal offset = m_scaledRect.width() - rightbar - leftbar;
        for (int i = 0; i <= FocusIndex && i < m_children.size(); ++i)
        {
            UIWidget* child = m_children.at(i);

            if (NOT_VISIBLE(child))
                continue;

            qreal width = m_fixedWidth > 1.0 ? m_fixedWidth : (child->m_scaledRect.size().width() * child->GetHorizontalZoom()) + m_spacingX;
            offset -= width;

            if (i < FocusIndex)
                leftoffset -= width;
        }

        position += QPointF(lefttoright ? leftbar : - leftbar, 0);

        bool onscreen = m_fullyDrawnWidgets.contains(FocusWidget);

        // handle moving left
        if (!onscreen && offset > m_lastOffset && m_lastOffset < 0.0 && offset > 0.0)
            offset = leftoffset;

        // is widget already fully rendered within bounds?
        if (onscreen)
        {
            // if this has interrupted an animation, adjust for minor overruns
            if (m_lastOffset > 0)
                m_lastOffset = 0;
            position += QPointF(lefttoright ? m_lastOffset : -m_lastOffset, 0);
            m_finish = m_lastOffset;
        }
        // adjustment required
        else if (m_skip || offset < 0 || (offset >= 0 && m_lastOffset < 0))
        {
            if (m_easingCurve && !m_skip)
            {
                if (offset != m_finish)
                {
                    m_start     = m_lastOffset == 0.0 ? 0.0 : m_finish;
                    m_finish       = offset;
                    m_duration  = qAbs((m_finish - m_start) / m_speed);
                    m_startTime = TimeNow;
                }

                quint64 prog = TimeNow - m_startTime;

                if (prog <= m_duration)
                {
                    qreal progress = prog / m_duration;
                    progress = m_easingCurve->valueForProgress(progress);
                    offset = m_start + ((m_finish - m_start) * progress);
                }
            }

            position += QPointF(lefttoright ? offset : -offset, 0);
            m_lastOffset = offset;

            if (m_skip)
                m_finish = m_lastOffset;
        }
        else // reset
        {
            m_lastOffset = 0.0;
        }

        // clear list of fully rendered onscreen widgets
        m_fullyDrawnWidgets.clear();

        // draw group
        for (int i = 0; i < m_children.size(); ++i)
        {
            UIWidget* child = m_children.at(i);

            if (NOT_VISIBLE(child))
                continue;

            UIEffect::Centre centre = (UIEffect::Centre)child->GetCentre();
            QSizeF size = child->m_scaledRect.size();
            qreal width = size.width();
            qreal total = width * child->GetHorizontalZoom();
            qreal move  = m_fixedWidth > 1.0 ? m_fixedWidth : total + m_spacingX;

            qreal heightadj = vcenter ?    (height - size.height()) / 2.0 :
                              bottomtotop ? height - size.height() : 0.0;
            qreal widthadj  = m_fixedWidth > 1.0 ? 0.0 :
                              IsVertCentred(centre)  ? (total - width) / 2.0 :
                              IsRightCentred(centre) ?  total - width : 0.0;

            if (lefttoright)
                position += QPointF(widthadj, heightadj);
            else
                position += QPointF(-(move - widthadj), heightadj);

            if (position.x() + total > 1 &&
                position.x() <  m_scaledRect.width())
            {
                // save any widgets that are fully drawn
                if (position.x() >= leftbar &&
                    position.x() + total <= m_scaledRect.width() - rightbar)
                {
                    m_fullyDrawnWidgets.append(child);
                }

                // save the focus widget and render last (on top)
                if (child == FocusWidget)
                {
                    topwidget = child;
                    toppos    = position;
                }
                else
                {
                    child->SetScaledPosition(position);
                    child->Draw(TimeNow, Window, XOffset, YOffset);
                }
            }

            if (lefttoright)
                position += QPointF(move - widthadj, -heightadj);
            else
                position += QPointF(-widthadj, -heightadj);

            // stop drawing once we're beyond the end of the group area
            if ((lefttoright && (position.x() >= limit)) || (righttoleft && (position.x() < limit)))
                break;
        }
    }
    else if (m_groupType == Vertical && (toptobottom || bottomtotop))
    {
        qreal topbar    = m_selectionRange.top();
        qreal bottombar = m_scaledRect.height() - m_selectionRange.bottom();
        qreal width     = m_scaledRect.width();
        qreal limit     = m_scaledRect.top() + YOffset;

        if (toptobottom)
            limit += m_scaledRect.height();
        else
            position += QPointF(0.0, m_scaledRect.height());

        // calculate any offset
        qreal topoffset = 0.0;
        qreal offset = m_scaledRect.height() - topbar - bottombar;
        for (int i = 0; i <= FocusIndex && i < m_children.size(); ++i)
        {
            UIWidget* child = m_children.at(i);

            if (NOT_VISIBLE(child))
                continue;

            qreal height = m_fixedHeight > 1.0 ? m_fixedHeight : (child->m_scaledRect.size().height() * child->GetVerticalZoom()) + m_spacingY;
            offset -= height;

            if (i < FocusIndex)
                topoffset -= height;
        }

        position += QPointF(0.0, toptobottom ? topbar : -topbar);

        bool onscreen = m_fullyDrawnWidgets.contains(FocusWidget);

        // handle moving up
        if (!onscreen && offset > m_lastOffset && m_lastOffset < 0.0 && offset > 0.0)
            offset = topoffset;

        // is widget already fully rendered within bounds?
        if (onscreen)
        {
            // if this has interrupted an animation, adjust for minor overruns
            if (m_lastOffset > 0)
                m_lastOffset = 0;
            position += QPointF(0.0, toptobottom ? m_lastOffset : -m_lastOffset);
            m_finish = m_lastOffset;
        }
        // adjustment required
        else if (m_skip || offset < 0 || (offset >= 0 && m_lastOffset < 0))
        {
            if (m_easingCurve && !m_skip)
            {
                if (offset != m_finish)
                {
                    m_start     = m_lastOffset == 0.0 ? 0.0 : m_finish;
                    m_finish       = offset;
                    m_duration  = qAbs((m_finish - m_start) / m_speed);
                    m_startTime = TimeNow;
                }

                quint64 prog = TimeNow - m_startTime;

                if (prog <= m_duration)
                {
                    qreal progress = prog / m_duration;
                    progress = m_easingCurve->valueForProgress(progress);
                    offset = m_start + ((m_finish - m_start) * progress);
                }
            }

            position += QPointF(0.0, toptobottom ? offset : -offset);
            m_lastOffset = offset;

            if (m_skip)
                m_finish = m_lastOffset;
        }
        else // reset
        {
            m_lastOffset = 0.0;
        }

        // clear list of fully rendered onscreen widgets
        m_fullyDrawnWidgets.clear();

        // draw group
        for (int i = 0; i < m_children.size(); ++i)
        {
            UIWidget* child = m_children.at(i);

            if (NOT_VISIBLE(child))
                continue;

            UIEffect::Centre centre = (UIEffect::Centre)child->GetCentre();
            QSizeF size  = child->m_scaledRect.size();
            qreal height = size.height();
            qreal total  = height * child->GetVerticalZoom();
            qreal move   = m_fixedHeight > 1.0 ? m_fixedHeight : total + m_spacingY;

            qreal widthadj  = hcenter ?     (width - size.width()) / 2.0 :
                              righttoleft ?  width - size.width() : 0.0;
            qreal heightadj = m_fixedHeight > 1.0 ? 0.0 :
                              IsHorizCentred(centre)  ? (total - height) / 2.0 :
                              IsBottomCentred(centre) ?  total - height : 0.0;

            if (toptobottom)
                position += QPointF(widthadj, heightadj);
            else
                position += QPointF(widthadj, -(move - heightadj));

            // cull widgets outside of the grid area
            if (position.y() + total > 1 &&
                position.y() <  m_scaledRect.height())
            {
                // save any widgets that are fully drawn
                if (position.y() >= topbar &&
                    position.y() + total <= m_scaledRect.height() - bottombar)
                {
                    m_fullyDrawnWidgets.append(child);
                }

                // save the focus widget and render last (on top)
                if (child == FocusWidget)
                {
                    topwidget = child;
                    toppos   = position;
                }
                else
                {
                    child->SetScaledPosition(position);
                    child->Draw(TimeNow, Window, XOffset, YOffset);
                }
            }

            if (toptobottom)
                position += QPointF(-widthadj, move - heightadj);
            else
                position += QPointF(-widthadj, -heightadj);

            if ((toptobottom && (position.y() >= limit)) || (bottomtotop && (position.y() < limit)))
                break;
        }
    }
    else if (m_groupType == Grid)
    {
        qreal leftbar   = m_selectionRange.left();
        qreal rightbar  = m_scaledRect.width() - m_selectionRange.right();
        qreal topbar    = m_selectionRange.top();
        qreal bottombar = m_scaledRect.height() - m_selectionRange.bottom();

        if (righttoleft)
            position += QPointF(m_scaledRect.width() - m_fixedWidth, 0.0);

        if (bottomtotop)
            position += QPointF(0.0, m_scaledRect.height() - m_fixedHeight);

        // calculate any offset
        int row, col, rows, cols = 0;
        GetGridPosition(FocusWidget, row, col, rows, cols);

        qreal width      = m_scaledRect.width() - leftbar - rightbar;
        qreal height     = m_scaledRect.height() - topbar - bottombar;
        qreal top        = -row * m_fixedHeight;
        qreal left       = -col * m_fixedWidth;
        qreal bottom     = height - (m_fixedHeight * (row + 1));
        qreal right      = width - (m_fixedWidth * (col + 1));
        bool onscreen    = m_fullyDrawnWidgets.contains(FocusWidget);
        bool inxrange    = (left <= m_lastOffset) && (left >= m_lastOffset - width + m_fixedWidth);
        bool inyrange    = (top <= m_lastOffset2) && (top >= m_lastOffset2 - height + m_fixedHeight);
        qreal shiftright = 0.0;
        qreal shiftdown  = 0.0;

        if (!onscreen)
        {
            if ((right > m_lastOffset) && (m_lastOffset < 0.0))
                right = left;
            if ((bottom > m_lastOffset2) && (m_lastOffset2 < 0.0))
                bottom = top;
        }

        if (onscreen || inxrange)
        {
            shiftright = m_lastOffset;
        }
        else if (right < 0.0)
        {
            shiftright = lefttoright ? right : -right;
        }

        if (onscreen || inyrange)
        {
            shiftdown = m_lastOffset2;
        }
        else if (bottom < 0.0)
        {
            shiftdown = toptobottom ? bottom : -bottom;
        }

        if (m_easingCurve &&
            ((shiftdown < 0.0  || (shiftdown >= 0.0  && m_lastOffset < 0.0)) ||
            (shiftright < 0.0) || (shiftright >= 0.0 && m_lastOffset2 < 0.0)))
        {
            if (shiftright != m_finish || shiftdown != m_finish2)
            {
                m_start     = m_lastOffset  == 0.0 ? 0.0 : m_finish;
                m_start2    = m_lastOffset2 == 0.0 ? 0.0 : m_finish2;
                m_finish    = shiftright;
                m_finish2   = shiftdown;
                m_duration  = qAbs((m_finish - m_start) / m_speed);
                m_duration2 = qAbs((m_finish2 - m_start2) / m_speed);
                m_startTime = TimeNow;
            }

            if (m_skip)
                m_duration = m_duration2 = 0.0;

            quint64 prog = TimeNow - m_startTime;

            if (prog <= m_duration || prog <= m_duration2)
            {
                qreal progress1 = prog / m_duration;
                qreal progress2 = prog / m_duration2;
                progress1  = m_easingCurve->valueForProgress(progress1);
                progress2  = m_easingCurve->valueForProgress(progress2);
                shiftright = m_start  + ((m_finish  - m_start)  * progress1);
                shiftdown  = m_start2 + ((m_finish2 - m_start2) * progress2);
            }
        }

        position += QPointF(shiftright, shiftdown);
        m_lastOffset = shiftright;
        m_lastOffset2 = shiftdown;

        // reset
        m_fullyDrawnWidgets.clear();

        qreal adjleft       = lefttoright ? leftbar : -leftbar;
        qreal adjtop        = toptobottom ? topbar  : -topbar;
        qreal completetop   = topbar - 1;
        qreal cullbottom    = m_scaledRect.height() - 1;
        qreal completebot   = m_scaledRect.height() - bottombar + 1;
        qreal completeleft  = leftbar - 1;
        qreal cullright     = m_scaledRect.width() - 1;
        qreal completeright = m_scaledRect.width() - rightbar + 1;

        // draw group
        for (int i = 0, x = 0, y = 0; i < m_children.size(); ++i)
        {
            UIWidget* child = m_children.at(i);

            if (NOT_VISIBLE(child))
                continue;

            // centre the child within the bounding rect
            QSizeF size = child->m_scaledRect.size();
            qreal xadj = (m_fixedWidth - size.width()) / 2.0;
            qreal yadj = (m_fixedHeight - size.height()) / 2.0;

            // row wrap
            if (x >= cols)
            {
                position += QPointF(lefttoright ? -m_fixedWidth * cols : m_fixedWidth * cols, 0.0);
                position += QPointF(0.0, toptobottom ? m_fixedHeight : -m_fixedHeight);

                if (y >= rows)
                    break;

                x = 0;
                y++;
            }

            position += QPointF(xadj + adjleft, yadj + adjtop);

            // cull widgets outside of the group area
            if (position.y() + m_fixedHeight > 1 && position.y() <  cullbottom &&
                position.x() + m_fixedWidth > 1 && position.x() < cullright)
            {
                // save any widgets that are fully drawn within the selection area
                if (position.y() >= completetop &&
                    position.y() + m_fixedHeight <= completebot &&
                    position.x() >= completeleft &&
                    position.x() + m_fixedWidth <= completeright)
                {
                    m_fullyDrawnWidgets.append(child);
                }

                // save the focus widget and render last (on top)
                if (child == FocusWidget)
                {
                    topwidget = child;
                    toppos    = position;
                }
                else
                {
                    child->SetScaledPosition(position);
                    child->Draw(TimeNow, Window, XOffset, YOffset);
                }
            }

            position += QPointF(-xadj + (lefttoright ? m_fixedWidth : -m_fixedWidth) - adjleft,
                               -yadj - adjtop);

            x++;
        }
    }

    // reset skip
    m_skip = false;

    // draw the focus widget on top
    if (topwidget)
    {
        topwidget->SetScaledPosition(toppos);
        topwidget->Draw(TimeNow, Window, XOffset, YOffset);
    }
}

bool UIGroup::Finalise(void)
{
    if (m_groupType != None)
    {
        foreach (UIWidget* child, m_children)
        {
            if (!child->IsTemplate() && !child->IsDecoration() && !child->IsDetached())
                child->SetPosition(0.0, 0.0);
        }
    }

    return UIWidget::Finalise();
}

bool UIGroup::AutoSelectFocusWidget(int Index)
{
    bool invert = (m_alignment & Qt::AlignBottom) ||
                  (m_alignment & Qt::AlignRight);
    bool ignore = m_parent ? m_parent->Type() != kUIGroupType : true;
    ignore |= m_groupType == None;

    if (!ignore)
    {
        UIGroup* parentgroup = static_cast<UIGroup*>(m_parent);
        if (parentgroup)
            ignore = parentgroup->GetGroupType() != m_groupType && m_groupType != Grid;
    }

    if (!ignore)
    {
        int direction = GetDirection();

        if (invert)
        {
            if      (direction == Torc::Down)  direction = Torc::Up;
            else if (direction == Torc::Up)    direction = Torc::Down;
            else if (direction == Torc::Left)  direction = Torc::Right;
            else if (direction == Torc::Right) direction = Torc::Left;
        }

        bool increasing = direction == Torc::Down || direction == Torc::Right;

        if (m_lastChildWithFocus && m_children.contains(m_lastChildWithFocus))
        {
            if (m_groupType == Horizontal || m_groupType == Vertical)
            {
                if (!increasing && FirstFocusWidget() == m_lastChildWithFocus)
                    m_lastChildWithFocus = LastFocusWidget();
                else if (increasing && LastFocusWidget() == m_lastChildWithFocus)
                    m_lastChildWithFocus = FirstFocusWidget();
            }
            else if (m_groupType == Grid)
            {
                int row, col, rows, cols = 0;
                GetGridPosition(m_lastChildWithFocus, row, col, rows, cols);
                bool select = false;

                if (direction == Torc::Right && col != 0)
                {
                    select = true;
                    col = cols -1;
                }
                else if (direction == Torc::Left && col != (cols - 1))
                {
                    select = true;
                    col = 0;
                }
                else if (direction == Torc::Down && row != 0)
                {
                    select = true;
                    row = rows - 1;
                }
                else if (direction == Torc::Up && row != (rows - 1))
                {
                    select = true;
                    row = 0;
                }

                if (select)
                {
                    bool dummy;
                    UIWidget* child = SelectChildInGrid(direction, row, col, rows, cols,
                                                        true, true, dummy);
                    if (child)
                        m_lastChildWithFocus = child;
                }
            }
        }
        else
        {
            if (!increasing && (m_groupType == Horizontal || m_groupType == Vertical))
            {
                m_lastChildWithFocus = LastFocusWidget();
            }
            else if (m_groupType == Grid)
            {
                int row, col, rows, cols = 0;
                GetGridPosition(m_lastChildWithFocus, row, col, rows, cols);
                bool dummy;
                UIWidget* child = SelectChildInGrid(direction,
                                                    direction == Torc::Down ? rows - 1 : 0,
                                                    direction == Torc::Right ? cols - 1 : 0,
                                                    rows, cols, true, true, dummy);
                if (child)
                    m_lastChildWithFocus = child;
            }
        }
    }

    return UIWidget::AutoSelectFocusWidget(Index);
}

inline UIWidget* UIGroup::FindInColumn(int Row, int TotalRows, int Column, int TotalColumns)
{
    for (int j = Row - 1, k = Row; j > -1 || k < TotalRows; --j, ++k)
    {
        if (k < TotalRows)
        {
            UIWidget* child = GetFocusableChild((k * TotalColumns) + Column);
            if (child)
                return child;
        }

        if (j > -1)
        {
            UIWidget* child = GetFocusableChild((j * TotalColumns) + Column);
            if (child)
                return child;
        }

    }

    return NULL;
}

inline UIWidget* UIGroup::FindInRow(int Row, int TotalRows, int Column, int TotalColumns)
{
    for (int j = Column - 1, k = Column; j > -1 || k < (int)TotalColumns; --j, ++k)
    {
        if (k < TotalColumns)
        {
            UIWidget* child = GetFocusableChild((Row * TotalColumns) + k);
            if (child)
                return child;
        }

        if (j > -1)
        {
            UIWidget* child = GetFocusableChild((Row * TotalColumns) + j);
            if (child)
                return child;
        }
    }

    return NULL;
}

UIWidget* UIGroup::SelectChildInGrid(int  Direction, int Row, int Column,
                                     int TotalRows,  int TotalColumns,
                                     bool WrapVertical, bool WrapHorizontal,
                                     bool &PassUp)
{
    if (TotalRows < 1 || TotalColumns < 1)
        return NULL;

    if (Direction      == Torc::Left)
        Column = Column == 0 ? TotalColumns - 1 : Column - 1;
    else if (Direction == Torc::Right)
        Column = Column == TotalColumns - 1 ? 0 : Column + 1;
    else if (Direction == Torc::Up)
        Row = Row == 0 ? TotalRows - 1 : Row - 1;
    else if (Direction == Torc::Down)
        Row = Row == TotalRows - 1 ? 0 : Row + 1;

    if (Direction == Torc::Left)
    {
        for (int i = Column; i > -1; --i)
        {
            UIWidget* child = FindInColumn(Row, TotalRows, i, TotalColumns);
            if (child)
                return child;
        }

        if (!WrapHorizontal)
        {
            PassUp = true;
            return NULL;
        }

        for (int i = TotalColumns - 1; i > Column; --i)
        {
            UIWidget* child = FindInColumn(Row, TotalRows, i, TotalColumns);
            if (child)
                return child;
        }
    }
    else if (Direction == Torc::Right)
    {
        for (int i = Column; i < TotalColumns; ++i)
        {
            UIWidget* child = FindInColumn(Row, TotalRows, i, TotalColumns);
            if (child)
                return child;
        }

        if (!WrapHorizontal)
        {
            PassUp = true;
            return NULL;
        }

        for (int i = 0; i < Column; ++i)
        {
            UIWidget* child = FindInColumn(Row, TotalRows, i, TotalColumns);
            if (child)
                return child;
        }
    }
    else if (Direction == Torc::Up)
    {
        for (int i = Row; i > -1; --i)
        {
            UIWidget* child = FindInRow(i, TotalRows, Column, TotalColumns);
            if (child)
                return child;
        }

        if (!WrapVertical)
        {
            PassUp = true;
            return NULL;
        }

        for (int i = TotalRows - 1; i > Row; --i)
        {
            UIWidget* child = FindInRow(i, TotalRows, Column, TotalColumns);
            if (child)
                return child;
        }
    }
    else if (Direction == Torc::Down)
    {
        for (int i = Row; i < TotalRows; ++i)
        {
            UIWidget* child = FindInRow(i, TotalRows, Column, TotalColumns);
            if (child)
                return child;
        }

        if (!WrapVertical)
        {
            PassUp = true;
            return NULL;
        }

        for (int i = 0; i < Row; ++i)
        {
            UIWidget* child = FindInRow(i, TotalRows, Column, TotalColumns);
            if (child)
                return child;
        }
    }

    return NULL;
}

static class UIGroupFactory : public WidgetFactory
{
    void RegisterConstructor(QScriptEngine *Engine)
    {
        if (!Engine)
            return;

        QScriptValue constructor = Engine->newFunction(UIGroupConstructor);
        Engine->globalObject().setProperty("UIGroup", constructor);
    }

    UIWidget* Create(const QString &Type, UIWidget *Root, UIWidget *Parent,
                     const QString &Name, int Flags)
    {
        if (Type == "group")
            return new UIGroup(Root, Parent, Name, Flags);
        return NULL;
    }

} UIWidgetFactory;

