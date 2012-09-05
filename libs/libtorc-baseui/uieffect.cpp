#include "uieffect.h"

UIEffect::UIEffect()
  : m_alpha(1.0),
    m_hZoom(1.0),
    m_vZoom(1.0),
    m_rotation(0.0),
    m_centre(Middle),
    m_hReflecting(false),
    m_hReflection(0.0),
    m_vReflecting(false),
    m_vReflection(0.0)
{
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

