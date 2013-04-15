/* Class UIEffect
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

// Torc
#include "uiwidget.h"
#include "uieffect.h"

UIEffect::UIEffect()
  : m_alpha(1.0),
    m_color(1.0),
    m_hZoom(1.0),
    m_vZoom(1.0),
    m_rotation(0.0),
    m_centre(Middle),
    m_hReflecting(false),
    m_hReflection(0.0),
    m_vReflecting(false),
    m_vReflection(0.0),
    m_detached(false),
    m_decoration(false)
{
}

void UIEffect::ParseEffect(QDomElement &Element, UIEffect *Effect)
{
    if (!Effect)
        return;

    QString alphas  = Element.attribute("alpha");
    QString colors  = Element.attribute("color");
    QString hzoom   = Element.attribute("horizontalzoom");
    QString vzoom   = Element.attribute("verticalzoom");
    QString zoom    = Element.attribute("zoom");
    QString angle   = Element.attribute("rotation");
    QString centres = Element.attribute("centre").toLower();
    QString hrefls  = Element.attribute("hreflection");
    QString vrefls  = Element.attribute("vreflection");
    QString detach  = Element.attribute("detached");
    QString decore  = Element.attribute("decoration");

    bool ok = false;

    if (!alphas.isEmpty())
    {
        qreal alphaf = alphas.toFloat(&ok);
        if (ok && alphaf >= 0.0 && alphaf <= 1.0)
            Effect->m_alpha = alphaf;
    }

    if (!colors.isEmpty())
    {
        qreal colorf = colors.toFloat(&ok);
        if (ok && colorf >= 0.0 && colorf <= 1.0)
            Effect->m_color = colorf;
    }

    if (!hzoom.isEmpty())
    {
        qreal hzoomf = hzoom.toFloat(&ok);
        if (ok && hzoomf >= 0.0 && hzoomf <= 1.0)
            Effect->m_hZoom = hzoomf;
    }

    if (!vzoom.isEmpty())
    {
        qreal vzoomf = vzoom.toFloat(&ok);
        if (ok && vzoomf >= 0.0 && vzoomf <= 1.0)
            Effect->m_vZoom = vzoomf;
    }

    if (!zoom.isEmpty())
    {
        qreal zoomf = zoom.toFloat(&ok);
        if (ok && zoomf >= 0.0 && zoomf <= 1.0)
        {
            Effect->m_hZoom = zoomf;
            Effect->m_vZoom = zoomf;
        }
    }

    if (!angle.isEmpty())
    {
        qreal anglef = angle.toFloat(&ok);
        if (ok)
            Effect->m_rotation = anglef;
    }

    if (!centres.isEmpty())
    {
        if (centres      == "topleft")     Effect->m_centre = UIEffect::TopLeft;
        else if (centres == "top")         Effect->m_centre = UIEffect::Top;
        else if (centres == "topright")    Effect->m_centre = UIEffect::TopRight;
        else if (centres == "left")        Effect->m_centre = UIEffect::Left;
        else if (centres == "middle")      Effect->m_centre = UIEffect::Middle;
        else if (centres == "right")       Effect->m_centre = UIEffect::Right;
        else if (centres == "bottomleft")  Effect->m_centre = UIEffect::BottomLeft;
        else if (centres == "bottom")      Effect->m_centre = UIEffect::Bottom;
        else if (centres == "bottomright") Effect->m_centre = UIEffect::BottomRight;
    }

    if (!hrefls.isEmpty())
    {
        qreal hrefl = hrefls.toFloat(&ok);
        if (ok)
        {
            Effect->m_hReflecting = true;
            Effect->m_hReflection = hrefl;
        }

    }

    if (!vrefls.isEmpty())
    {
        qreal vrefl = vrefls.toFloat(&ok);
        if (ok)
        {
            Effect->m_vReflecting = true;
            Effect->m_vReflection = vrefl;
        }
    }

    if (!detach.isEmpty())
    {
        Effect->m_detached = UIWidget::GetBool(detach);
    }

    if (!decore.isEmpty())
    {
        Effect->m_decoration = UIWidget::GetBool(decore);
    }
}

QPointF UIEffect::GetCentre(const QRectF *Rect) const
{
    if (!Rect)
        return QPointF(0, 0);

    qreal x = 0;
    qreal y = 0;
    if (Middle == m_centre || Top == m_centre || Bottom == m_centre)
        x += Rect->width() / 2.0;
    if (Middle == m_centre || Left == m_centre || Right == m_centre)
        y += Rect->height() / 2.0;
    if (Right == m_centre  || TopRight == m_centre || BottomRight == m_centre)
        x += Rect->width();
    if (Bottom == m_centre || BottomLeft == m_centre || BottomRight == m_centre)
        y += Rect->height();
    return QPointF(x, y);
}

