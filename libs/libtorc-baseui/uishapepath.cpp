/* Class UIShapePath
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
#include <QDomElement>

// Torc
#include "torclogging.h"
#include "uiimage.h"
#include "uiwidget.h"
#include "uishapepath.h"

UIPath::UIPath()
  : m_outline(false),
    m_fill(false),
    m_blur(0)
{
}

UIShapePath::UIShapePath(UIWidget *Parent)
  : QObject(),
    TorcReferenceCounter(),
    m_parent(Parent),
    m_currentIndex(-1),
    m_finalised(false),
    m_blur(0)
{
}

UIShapePath::~UIShapePath()
{
    m_paths.clear();
}

void UIShapePath::Finalise(void)
{
    m_finalised = true;
}

bool UIShapePath::IsFinal(void)
{
    return m_finalised;
}

void UIShapePath::CheckPath(void)
{
    if (m_currentIndex < 0)
    {
        m_paths.append(UIPath());
        m_currentIndex++;
    }
}

void UIShapePath::ClosePath(void)
{
    m_paths.append(UIPath());
    m_currentIndex++;
}

void UIShapePath::CloseSubpath(void)
{
    CheckPath();
    m_paths[m_currentIndex].m_painterPath.closeSubpath();
}

void UIShapePath::MoveTo(const QPointF &Point)
{
    CheckPath();
    m_paths[m_currentIndex].m_painterPath.moveTo(Point);
}

void UIShapePath::ArcTo(const QRectF &Rect, qreal Angle, qreal Length)
{
    CheckPath();
    m_paths[m_currentIndex].m_painterPath.arcTo(Rect, Angle, Length);
}

void UIShapePath::AddRect(const QRectF &Rect)
{
    CheckPath();
    m_paths[m_currentIndex].m_painterPath.addRect(Rect);
}

void UIShapePath::AddRoundedRect(const QRectF &Rect, qreal XRadius, qreal YRadius)
{
    CheckPath();
    m_paths[m_currentIndex].m_painterPath.addRoundedRect(Rect, XRadius, YRadius);
}

void UIShapePath::LineTo(const QPointF &Point)
{
    CheckPath();
    m_paths[m_currentIndex].m_painterPath.lineTo(Point);
}

void UIShapePath::AddEllipse(const QRectF &Rect)
{
    CheckPath();
    m_paths[m_currentIndex].m_painterPath.addEllipse(Rect);
}

void UIShapePath::CubicTo(const QPointF &Control1, const QPointF &Control2, const QPointF &Endpoint)
{
    CheckPath();
    m_paths[m_currentIndex].m_painterPath.cubicTo(Control1, Control2, Endpoint);
}

void UIShapePath::SetFillrule(int Fill)
{
    CheckPath();
    m_paths[m_currentIndex].m_painterPath.setFillRule((Qt::FillRule)Fill);
}

void UIShapePath::SetPenColor(const QColor &Color)
{
    CheckPath();
    m_paths[m_currentIndex].m_pen.setColor(Color);
}

void UIShapePath::SetPenWidth(int Width)
{
    CheckPath();
    m_paths[m_currentIndex].m_pen.setWidth(Width);
}

void UIShapePath::SetPenStyle(int PenStyle)
{
    CheckPath();
    m_paths[m_currentIndex].m_pen.setStyle((Qt::PenStyle)PenStyle);
}

void UIShapePath::SetPenJoinStyle(int PenJoinStyle)
{
    CheckPath();
    m_paths[m_currentIndex].m_pen.setJoinStyle((Qt::PenJoinStyle)PenJoinStyle);
}

void UIShapePath::SetPenCapStyle(int PenCapStyle)
{
    CheckPath();
    m_paths[m_currentIndex].m_pen.setCapStyle((Qt::PenCapStyle)PenCapStyle);
}

void UIShapePath::SetBrushColor(const QColor &Color)
{
    CheckPath();
    m_paths[m_currentIndex].m_brush.setColor(Color);
}

void UIShapePath::SetBrushStyle(int BrushStyle)
{
    CheckPath();
    m_paths[m_currentIndex].m_brush.setStyle((Qt::BrushStyle)BrushStyle);
}

void UIShapePath::SetBrush(const QBrush &Brush)
{
    CheckPath();
    m_paths[m_currentIndex].m_brush = Brush;
}

void UIShapePath::SetPathBlur(int Radius)
{
    CheckPath();
    m_paths[m_currentIndex].m_blur = (int)(Radius * m_parent->GetXScale());
}

void UIShapePath::SetBlur(int Radius)
{
    m_blur = (int)(Radius * m_parent->GetXScale());
}

bool UIShapePath::ParsePath(QDomElement *Element)
{
    if (!Element || !m_parent)
        return false;

    m_paths.append(UIPath());
    m_currentIndex++;

    bool ok = false;
    bool ok2 = false;

    for (QDomNode node = Element->firstChild(); !node.isNull(); node = node.nextSibling())
    {
        QDomElement element = node.toElement();
        if (element.isNull())
            continue;

        // NB Paths are not automatically closed
        if (element.tagName() == "closesubpath")
        {
            CloseSubpath();
        }
        else if (element.tagName() == "moveto")
        {
            QString points = element.attribute("point");
            if (!points.isEmpty())
            {
                QPointF point = m_parent->GetScaledPointF(points);
                MoveTo(point);
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, "Incomplete 'moveto' tag");
            }
        }
        else if (element.tagName() == "arcto")
        {
            QString rects   = element.attribute("rect");
            QString angles  = element.attribute("startangle");
            QString lengths = element.attribute("sweeplength");

            if (!rects.isEmpty() && !angles.isEmpty() && !lengths.isEmpty())
            {
                QRectF rect  = m_parent->GetScaledRect(rects);
                qreal angle  = angles.toFloat(&ok);
                qreal length = lengths.toFloat(&ok2);

                if (ok && ok2)
                    ArcTo(rect, angle, length);
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, "Incomplete 'arcto' tag");
            }
        }
        else if (element.tagName() == "addrect")
        {
            QString rects = element.attribute("rect");

            if (!rects.isEmpty())
            {
                QRectF rect = m_parent->GetScaledRect(rects);
                AddRect(rect);
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, "Incomplete 'addrect' tag");
            }
        }
        else if (element.tagName() == "addroundedrect")
        {
            QString rects = element.attribute("rect");
            QString xrad  = element.attribute("xradius");
            QString yrad  = element.attribute("yradius");

            if (!rects.isEmpty() && !xrad.isEmpty() && !yrad.isEmpty())
            {
                QRectF rect = m_parent->GetScaledRect(rects);
                qreal  radx = xrad.toFloat(&ok) * m_parent->GetXScale();
                qreal  rady = yrad.toFloat(&ok2) * m_parent->GetYScale();

                if (ok && ok2)
                    AddRoundedRect(rect, radx, rady);
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, "Incomplete 'addroundedrect' tag");
            }
        }
        else if (element.tagName() == "lineto")
        {
            QString points = element.attribute("point");
            if (!points.isEmpty())
            {
                QPointF point = m_parent->GetScaledPointF(points);
                LineTo(point);
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, "Incomplete 'lineto' tag");
            }
        }
        else if (element.tagName() == "addellipse")
        {
            QString rects = element.attribute("rect");

            if (!rects.isEmpty())
            {
                QRectF rect = m_parent->GetScaledRect(rects);
                AddEllipse(rect);
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, "Incomplete 'addellipse' tag");
            }
        }
        else if (element.tagName() == "cubicto")
        {
            QString c1s  = element.attribute("control1");
            QString c2s  = element.attribute("control2");
            QString ends = element.attribute("endpoint");

            if (!c1s.isEmpty() && !c2s.isEmpty() && !ends.isEmpty())
            {
                QPointF c1  = m_parent->GetScaledPointF(c1s);
                QPointF c2  = m_parent->GetScaledPointF(c2s);
                QPointF end = m_parent->GetScaledPointF(ends);
                CubicTo(c1, c2, end);
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, "Incomplete 'cubicto' tag");
            }
        }
        else if (element.tagName() == "fill")
        {
            QString rules = element.attribute("rule").trimmed();

            if (!rules.isEmpty())
            {
                if (rules == "OddEvenFill")
                    SetFillrule(Qt::OddEvenFill);
                else if (rules == "WindingFill")
                    SetFillrule(Qt::WindingFill);
            }
        }
        else if (element.tagName() == "pen")
        {
            m_paths[m_currentIndex].m_outline = true;

            QString colors = element.attribute("color");
            QString widths = element.attribute("width");
            QString styles = element.attribute("penstyle");
            QString joins  = element.attribute("penjoinstyle");
            QString caps   = element.attribute("pencapstyle");

            if (!colors.isEmpty())
                SetPenColor(UIWidget::GetQColor(colors));

            if (!widths.isEmpty())
            {
                qreal width = widths.toFloat(&ok) * m_parent->GetXScale();
                if (ok)
                    SetPenWidth(width);
            }

            if (!styles.isEmpty())
            {
                const QMetaObject &mo = staticQtMetaObject;
                int enum_index        = mo.indexOfEnumerator("PenStyle");
                QMetaEnum metaEnum    = mo.enumerator(enum_index);
                SetPenStyle(metaEnum.keyToValue(styles.toLatin1()));
            }

            if (!joins.isEmpty())
            {
                const QMetaObject &mo = staticQtMetaObject;
                int enum_index        = mo.indexOfEnumerator("PenJoinStyle");
                QMetaEnum metaEnum    = mo.enumerator(enum_index);
                SetPenJoinStyle(metaEnum.keyToValue(joins.toLatin1()));
            }

            if (!caps.isEmpty())
            {
                const QMetaObject &mo = staticQtMetaObject;
                int enum_index        = mo.indexOfEnumerator("PenCapStyle");
                QMetaEnum metaEnum    = mo.enumerator(enum_index);
                SetPenCapStyle(metaEnum.keyToValue(caps.toLatin1()));
            }
        }
        else if (element.tagName() == "brush")
        {
            m_paths[m_currentIndex].m_fill = true;

            QString colors = element.attribute("color");
            QString styles = element.attribute("brushstyle");

            if (!colors.isEmpty())
                SetBrushColor(UIWidget::GetQColor(colors));

            if (!styles.isEmpty())
            {
                const QMetaObject &mo = staticQtMetaObject;
                int enum_index        = mo.indexOfEnumerator("BrushStyle");
                QMetaEnum metaEnum    = mo.enumerator(enum_index);
                SetBrushStyle(metaEnum.keyToValue(styles.toLatin1()));
            }

            for (QDomNode node2 = element.firstChild(); !node2.isNull(); node2 = node2.nextSibling())
            {
                QDomElement element2 = node2.toElement();
                if (element2.isNull())
                    continue;

                if (element2.tagName() == "gradient")
                {
                    QString type = element2.attribute("type");

                    if (type == "linear")
                        ParseLinearGradient(&element2);
                }
            }
        }
        else if (element.tagName() == "fixedblur")
        {
            QString blurs = element.attribute("radius");
            if (!blurs.isEmpty())
            {
                int blur = blurs.toInt(&ok);
                if (ok)
                    SetPathBlur(blur);
            }
        }
    }

    return true;
}

void UIShapePath::ParseLinearGradient(QDomElement *Element)
{
    if (!Element)
        return;

    QString starts = Element->attribute("start");
    QString stops  = Element->attribute("stop");

    if (starts.isEmpty() || stops.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Incomplete 'gradient' tag");
        return;
    }

    QPointF start = m_parent->GetScaledPointF(starts);
    QPointF stop  = m_parent->GetScaledPointF(stops);

    QLinearGradient gradient(start, stop);

    for (QDomNode node = Element->firstChild(); !node.isNull(); node = node.nextSibling())
    {
        QDomElement element = node.toElement();
        if (element.isNull())
            continue;

        if (element.tagName() == "stop")
        {
            QString points = element.attribute("point");
            QString colors = element.attribute("color");

            if (!points.isEmpty() && !colors.isEmpty())
            {
                bool ok = false;
                QColor color = UIWidget::GetQColor(colors);
                qreal point = points.toFloat(&ok);

                if (ok)
                    gradient.setColorAt(point, color);
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, "Incomplete 'stop' tag");
            }
        }
    }

    m_paths[m_currentIndex].m_brush = QBrush(gradient);
}

void UIShapePath::RenderPath(QPaintDevice *Device)
{
    if (!Device)
        return;

    for (int i = 0; i < m_paths.size(); ++i)
    {
        QPainter painter(Device);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(m_paths[i].m_outline ? m_paths[i].m_pen : Qt::NoPen);
        painter.setBrush(m_paths[i].m_fill ? m_paths[i].m_brush : Qt::NoBrush);
        painter.drawPath(m_paths[i].m_painterPath);
        painter.end();

        if (m_paths[i].m_blur)
        {
            QImage* image = static_cast<QImage*>(Device);
            if (image)
                UIImage::Blur(image, m_paths[i].m_blur);
        }
    }

    if (m_blur)
    {
        QImage* image = static_cast<QImage*>(Device);
        if (image)
            UIImage::Blur(image, m_blur);
    }
}
