#ifndef UIEFFECT_H
#define UIEFFECT_H

//Qt
#include <QDomElement>
#include <QPointF>
#include <QRect>

#define IsLeftCentred(Centre)   ((Centre == UIEffect::TopLeft) || (Centre == UIEffect::Left) || (Centre == UIEffect::BottomLeft))
#define IsVertCentred(Centre)   ((Centre == UIEffect::Top) || (Centre == UIEffect::Middle) || (Centre == UIEffect::Bottom))
#define IsRightCentred(Centre)  ((Centre == UIEffect::TopRight) || (Centre == UIEffect::Right) || (Centre == UIEffect::BottomRight))
#define IsTopCentred(Centre)    ((Centre == UIEffect::Top) || (Centre == UIEffect::TopLeft) || (Centre == UIEffect::TopRight))
#define IsHorizCentred(Centre)  ((Centre == UIEffect::Left) || (Centre == UIEffect::Middle) || (Centre == UIEffect::Right))
#define IsBottomCentred(Centre) ((Centre == UIEffect::Bottom) || (Centre == UIEffect::BottomLeft) || (Centre == UIEffect::BottomRight))

class UIEffect
{
  public:
    enum Centre { TopLeft,    Top,    TopRight,
                  Left,       Middle, Right,
                  BottomLeft, Bottom, BottomRight };

    UIEffect();
    static  void ParseEffect (QDomElement &Element, UIEffect *Effect);
    QPointF      GetCentre   (const QRectF *Rect) const;

    qreal  m_alpha;
    qreal  m_color;
    qreal  m_hZoom;
    qreal  m_vZoom;
    qreal  m_rotation;
    Centre m_centre;
    bool   m_hReflecting;
    qreal  m_hReflection;
    bool   m_vReflecting;
    qreal  m_vReflection;
    bool   m_detached;
};

#endif // UIEFFECT_H
