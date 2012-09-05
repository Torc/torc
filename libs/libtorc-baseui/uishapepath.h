#ifndef UISHAPEPATH_H
#define UISHAPEPATH_H

// Qt
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QBrush>

// Torc
#include "torcreferencecounted.h"

class QDomElement;
class QPaintDevice;
class UIWidget;

class UIPath
{
  public:
    UIPath();

    QPainterPath  m_painterPath;
    QPen          m_pen;
    QBrush        m_brush;
    bool          m_outline;
    bool          m_fill;
    int           m_blur;
};

class UIShapePath : public TorcReferenceCounter
{
  public:
    UIShapePath(UIWidget *Parent);
    virtual ~UIShapePath();

    void          Finalise             (void);
    bool          IsFinal              (void);

    void          CheckPath            (void);
    void          ClosePath            (void);
    void          CloseSubpath         (void);
    void          MoveTo               (const QPointF &Point);
    void          ArcTo                (const QRectF &Rect, qreal Angle, qreal Length);
    void          AddRect              (const QRectF &Rect);
    void          AddRoundedRect       (const QRectF &Rect, qreal XRadius, qreal YRadius);
    void          LineTo               (const QPointF &Point);
    void          AddEllipse           (const QRectF &Rect);
    void          CubicTo              (const QPointF &Control1, const QPointF &Control2, const QPointF &Endpoint);
    void          SetFillrule          (int   Fill);
    void          SetPenColor          (const QColor &Color);
    void          SetPenWidth          (int   Width);
    void          SetPenStyle          (int   PenStyle);
    void          SetPenJoinStyle      (int   PenJoinStyle);
    void          SetPenCapStyle       (int   PenCapStyle);
    void          SetBrushColor        (const QColor &Color);
    void          SetBrushStyle        (int   BrushStyle);
    void          SetBrush             (const QBrush &Brush);
    void          SetPathBlur          (int   Radius);
    void          SetBlur              (int   Radius);

    bool          ParsePath            (QDomElement *Element);
    void          RenderPath           (QPaintDevice *Device);
    void          ParseLinearGradient  (QDomElement *Element);

  protected:
    UIWidget     *m_parent;
    QList<UIPath> m_paths;
    int           m_currentIndex;
    bool          m_finalised;
    int           m_blur;
};

#endif // UISHAPEPATH_H
