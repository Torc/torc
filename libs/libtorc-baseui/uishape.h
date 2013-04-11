#ifndef UISHAPE_H
#define UISHAPE_H

// Torc
#include "uiwidget.h"

class UIShapePath;

class UIShape : public UIWidget
{
    Q_OBJECT

  public:
    UIShape(UIWidget *Root, UIWidget *Parent, const QString &Name, int Flags);
    virtual ~UIShape();

    virtual     bool  DrawSelf          (UIWindow* Window, qreal XOffset, qreal YOffset);
    virtual     UIWidget* CreateCopy    (UIWidget *Parent, const QString &Newname = QString(""));
    virtual     void  CopyFrom          (UIWidget *Other);

    Q_INVOKABLE void  ClosePath         (void);
    Q_INVOKABLE void  CloseSubpath      (void);
    void              MoveTo            (const QPointF &Point);
    Q_INVOKABLE void  MoveTo            (qreal X, qreal Y);
    void              ArcTo             (const QRectF &Rect, qreal Angle, qreal Length);
    Q_INVOKABLE void  ArcTo             (qreal Left, qreal Top, qreal Width, qreal Height, qreal Angle, qreal Length);
    void              AddRect           (const QRectF &Rect);
    Q_INVOKABLE void  AddRect           (qreal Left, qreal Top, qreal Width, qreal Height);
    void              AddRoundedRect    (const QRectF &Rect, qreal XRadius, qreal YRadius);
    Q_INVOKABLE void  AddRoundedRect    (qreal Left, qreal Top, qreal Width, qreal Height, qreal XRadius, qreal YRadius);
    void              LineTo            (const QPointF &Point);
    Q_INVOKABLE void  LineTo            (qreal X, qreal Y);
    void              AddEllipse        (const QRectF &Rect);
    Q_INVOKABLE void  AddEllipse        (qreal Left, qreal Top, qreal Width, qreal Height);
    void              CubicTo           (const QPointF &Control1, const QPointF &Control2, const QPointF &Endpoint);
    Q_INVOKABLE void  CubicTo           (qreal Control1X, qreal Control1Y, qreal Control2X, qreal Control2Y, qreal EndpointX, qreal EndpointY);
    Q_INVOKABLE void  SetFillrule       (int   Fill);
    void              SetPenColor       (const QColor &Color);
    Q_INVOKABLE void  SetPenColor       (const QString &Color);
    Q_INVOKABLE void  SetPenWidth       (int   Width);
    Q_INVOKABLE void  SetPenStyle       (int   PenStyle);
    Q_INVOKABLE void  SetPenJoinStyle   (int   PenJoinStyle);
    Q_INVOKABLE void  SetPenCapStyle    (int   PenCapStyle);
    void              SetBrushColor     (const QColor &Color);
    Q_INVOKABLE void  SetBrushColor     (const QString &Color);
    Q_INVOKABLE void  SetBrushStyle     (int   BrushStyle);
    void              SetBrush          (const QBrush &Brush);
    Q_INVOKABLE void  SetPathBlur       (int   Radius);
    Q_INVOKABLE void  SetBlur           (int   Radius);

  protected:
    // UIWidget
    virtual     bool  InitialisePriv    (QDomElement *Element);

    UIShapePath*      GetPath           (void);

  protected:
    UIShapePath *m_path;

  private:
    Q_DISABLE_COPY(UIShape)
};

Q_DECLARE_METATYPE(UIShape*);

#endif // UISHAPE_H
