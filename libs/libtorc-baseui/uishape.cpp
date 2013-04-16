/* Class UIShape
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
#include "torclogging.h"
#include "uiwindow.h"
#include "uishapepath.h"
#include "uishape.h"

#define CHECK_STATUS \
if (!GetPath()) \
    return; \
if (m_path->IsFinal()) \
{ \
    LOG(VB_GENERAL, LOG_WARNING, "Painter path already finalised. Ignoring update."); \
    return; \
} \


static QScriptValue UIShapeConstructor(QScriptContext *conshape, QScriptEngine *engine)
{
    return WidgetConstructor<UIShape>(conshape, engine);
}

UIShape::UIShape(UIWidget *Root, UIWidget *Parent, const QString &Name, int Flags)
  : UIWidget(Root, Parent, Name, Flags),
    m_path(NULL)
{
}

UIShape::~UIShape()
{
    if (m_path)
        m_path->DownRef();
    m_path = NULL;
}

bool UIShape::DrawSelf(UIWindow *Window, qreal XOffset, qreal YOffset)
{
    if (!Window || !m_path)
        return false;

    m_path->Finalise();

    Window->DrawShape(m_effect, &m_scaledRect, m_positionChanged, m_path);
    return true;
}

UIShapePath* UIShape::GetPath(void)
{
    if (!m_path)
        m_path = new UIShapePath(this);
    return m_path;
}

bool UIShape::InitialisePriv(QDomElement *Element)
{
    if (Element->tagName() == "path")
    {
        if (GetPath())
            GetPath()->ParsePath(Element);
        return true;
    }

    if (Element->tagName() == "fixedblur")
    {
        QString blurs = Element->attribute("radius");
        if (!blurs.isEmpty())
        {
            bool ok;
            int blur = blurs.toInt(&ok);
            if (ok && GetPath())
                GetPath()->SetBlur(blur);
        }
        return true;
    }

    return false;
}

void UIShape::CopyFrom(UIWidget *Other)
{
    UIShape *shape = dynamic_cast<UIShape*>(Other);

    if (!shape)
    {
        LOG(LOG_INFO, LOG_ERR, "Failed to cast widget to UIShape.");
        return;
    }

    m_path = shape->m_path;
    if (m_path)
        m_path->UpRef();

    UIWidget::CopyFrom(Other);
}

UIWidget* UIShape::CreateCopy(UIWidget *Parent, const QString &Newname)
{
    bool isnew = !Newname.isEmpty();
    UIShape* shape = new UIShape(m_rootParent, Parent,
                                 isnew ? Newname : GetDerivedWidgetName(Parent->objectName()),
                                 (!isnew && IsTemplate()) ? WidgetFlagTemplate : WidgetFlagNone);
    shape->CopyFrom(this);
    return shape;
}

void UIShape::ClosePath(void)
{
    CHECK_STATUS;
    m_path->ClosePath();
}

void UIShape::CloseSubpath(void)
{
    CHECK_STATUS;
    m_path->CloseSubpath();
}

void UIShape::MoveTo(const QPointF &Point)
{
    CHECK_STATUS;
    m_path->MoveTo(Point);
}

void UIShape::MoveTo(qreal X, qreal Y)
{
    MoveTo(QPointF(X, Y));
}

void UIShape::ArcTo(const QRectF &Rect, qreal Angle, qreal Length)
{
    CHECK_STATUS;
    m_path->ArcTo(Rect, Angle, Length);
}

void UIShape::ArcTo(qreal Left, qreal Top, qreal Width, qreal Height, qreal Angle, qreal Length)
{
    ArcTo(QRectF(Left, Top, Width, Height), Angle, Length);
}

void UIShape::AddRect(const QRectF &Rect)
{
    CHECK_STATUS;
    m_path->AddRect(Rect);
}

void UIShape::AddRect(qreal Left, qreal Top, qreal Width, qreal Height)
{
    AddRect(QRectF(Left, Top, Width, Height));
}

void UIShape::AddRoundedRect(const QRectF &Rect, qreal XRadius, qreal YRadius)
{
    CHECK_STATUS;
    m_path->AddRoundedRect(Rect, XRadius, YRadius);
}

void UIShape::AddRoundedRect(qreal Left, qreal Top, qreal Width, qreal Height, qreal XRadius, qreal YRadius)
{
    AddRoundedRect(QRectF(Left, Top, Width, Height), XRadius, YRadius);
}

void UIShape::LineTo(const QPointF &Point)
{
    CHECK_STATUS;
    m_path->LineTo(Point);
}

void UIShape::LineTo(qreal X, qreal Y)
{
    LineTo(QPointF(X, Y));
}

void UIShape::AddEllipse(const QRectF &Rect)
{
    CHECK_STATUS;
    m_path->AddEllipse(Rect);
}

void UIShape::AddEllipse(qreal Left, qreal Top, qreal Width, qreal Height)
{
    AddEllipse(QRectF(Left, Top, Width, Height));
}

void UIShape::CubicTo(const QPointF &Control1, const QPointF &Control2, const QPointF &Endpoint)
{
    CHECK_STATUS;
    m_path->CubicTo(Control1, Control2, Endpoint);
}

void UIShape::CubicTo(qreal Control1X, qreal Control1Y, qreal Control2X, qreal Control2Y, qreal EndpointX, qreal EndpointY)
{
    CubicTo(QPointF(Control1X, Control1Y), QPointF(Control2X, Control2Y), QPointF(EndpointX, EndpointY));
}

void UIShape::SetFillrule(int Fill)
{
    CHECK_STATUS;
    m_path->SetFillrule(Fill);
}

void UIShape::SetPenColor(const QColor &Color)
{
    CHECK_STATUS;
    m_path->SetPenColor(Color);
}

void UIShape::SetPenColor(const QString &Color)
{
    SetPenColor(UIWidget::GetQColor(Color));
}

void UIShape::SetPenWidth(int Width)
{
    CHECK_STATUS;
    m_path->SetPenWidth(Width);
}

void UIShape::SetPenStyle(int PenStyle)
{
    CHECK_STATUS;
    m_path->SetPenStyle(PenStyle);
}

void UIShape::SetPenJoinStyle(int PenJoinStyle)
{
    CHECK_STATUS;
    m_path->SetPenJoinStyle(PenJoinStyle);
}

void UIShape::SetPenCapStyle(int PenCapStyle)
{
    CHECK_STATUS;
    m_path->SetPenCapStyle(PenCapStyle);
}

void UIShape::SetBrushColor(const QColor &Color)
{
    CHECK_STATUS;
    m_path->SetBrushColor(Color);
}

void UIShape::SetBrushColor(const QString &Color)
{
    SetBrushColor(UIWidget::GetQColor(Color));
}

void UIShape::SetBrushStyle(int BrushStyle)
{
    CHECK_STATUS;
    m_path->SetBrushStyle(BrushStyle);
}

void UIShape::SetBrush(const QBrush &Brush)
{
    CHECK_STATUS;
    m_path->SetBrush(Brush);
}

void UIShape::SetPathBlur(int Radius)
{
    CHECK_STATUS;
    m_path->SetPathBlur(Radius);
}

void UIShape::SetBlur(int Radius)
{
    CHECK_STATUS;
    m_path->SetBlur(Radius);
}

static class UIShapeFactory : public WidgetFactory
{
    void RegisterConstructor(QScriptEngine *Engine)
    {
        if (!Engine)
            return;

        QScriptValue constructor = Engine->newFunction(UIShapeConstructor);
        Engine->globalObject().setProperty("UIShape", constructor);
    }

    UIWidget* Create(const QString &Type, UIWidget *Root, UIWidget *Parent,
                     const QString &Name, int Flags)
    {
        if (Type == "shape")
            return new UIShape(Root, Parent, Name, Flags);
        return NULL;
    }

} UIShapeFactory;

