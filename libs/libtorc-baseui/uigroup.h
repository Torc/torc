#ifndef UIGROUP_H
#define UIGROUP_H

// Torc
#include "uiwidget.h"

class UIGroup : public UIWidget
{
    Q_OBJECT

  public:
    enum GroupType { None, Horizontal, Vertical, Grid };

    static  int     kUIGroupType;
    UIGroup(UIWidget *Root, UIWidget* Parent, const QString &Name, int Flags);
    virtual         ~UIGroup();
    bool            HandleAction      (int       Action);
    void            CopyFrom          (UIWidget *Other);
    bool            Draw              (quint64 TimeNow, UIWindow* Window, qreal XOffset, qreal YOffset);
    bool            Finalise          (void);
    bool            AutoSelectFocusWidget (int Index);

    Q_PROPERTY      (int   grouptype   READ GetGroupType      WRITE SetType)
    Q_PROPERTY      (bool  wrapSelection READ GetWrapSelection WRITE SetWrapSelection)
    Q_PROPERTY      (int   alignment   READ GetGroupAlignment WRITE SetAlignment)
    Q_PROPERTY      (qreal fixedwidth  READ GetFixedWidth     WRITE SetFixedWidth)
    Q_PROPERTY      (qreal fixedheight READ GetFixedHeight    WRITE SetFixedHeight)
    Q_PROPERTY      (qreal spacingX    READ GetSpacingX       WRITE SetSpacingX)
    Q_PROPERTY      (qreal spacingY    READ GetSpacingY       WRITE SetSpacingY)

  public slots:
    int             GetGroupType      (void);
    bool            GetWrapSelection  (void);
    int             GetGroupAlignment (void);
    qreal           GetFixedWidth     (void);
    qreal           GetFixedHeight    (void);
    qreal           GetSpacingX       (void);
    qreal           GetSpacingY       (void);
    void            SetType           (int Type);
    void            SetWrapSelection  (bool Wrap);
    void            SetAlignment      (int Alignment);
    void            SetFixedWidth     (qreal Width);
    void            SetFixedHeight    (qreal Height);
    void            SetSpacingX       (qreal Spacing);
    void            SetSpacingY       (qreal Spacing);
    void            SetSelectionRange (const QRectF &Range);
    void            SetSelectionRange (qreal Left, qreal Top, qreal Width, qreal Height);
    void            SetAnimationCurve (int EasingCurve);
    void            SetAnimationSpeed (qreal Speed);

  protected:
    virtual bool    InitialisePriv    (QDomElement *Element);
    virtual void    CreateCopy        (UIWidget *Parent);
    void            Validate          (void);
    UIWidget*       FirstFocusWidget  (void);
    UIWidget*       LastFocusWidget   (void);
    UIWidget*       GetFocusableChild (int Index);
    void            GetGridPosition   (UIWidget* Widget,
                                       int &Row, int &Column,
                                       int &TotalRows, int &TotalColumns);
    UIWidget*       SelectChildInGrid (int Direction, int Row, int Column,
                                       int TotalRows, int TotalColumns,
                                       bool WrapVertical, bool WrapHorizontal,
                                       bool &PassUp);
    UIWidget*       FindInColumn      (int Row, int TotalRows, int Column, int TotalColumns);
    UIWidget*       FindInRow         (int Row, int TotalRows, int Column, int TotalColumns);
    void            DrawGroup         (quint64 TimeNow, UIWindow* Window,
                                       qreal XOffset, qreal YOffset,
                                       int FocusIndex, UIWidget *FocusWidget);

  protected:
    GroupType       m_groupType;
    bool            m_wrapSelection;
    Qt::Alignment   m_alignment;
    qreal           m_fixedWidth;
    qreal           m_fixedHeight;
    qreal           m_spacingX;
    qreal           m_spacingY;

    QRectF          m_selectionRange;
    QEasingCurve*   m_easingCurve;
    qreal           m_speed;
    qreal           m_duration;
    quint64         m_startTime;
    qreal           m_start;
    qreal           m_finish;
    qreal           m_lastOffset;
    qreal           m_duration2;
    quint64         m_startTime2;
    qreal           m_start2;
    qreal           m_finish2;
    qreal           m_lastOffset2;
    bool            m_skip;
    QList<UIWidget*> m_fullyDrawnWidgets;

  protected:
    int             grouptype;
    bool            wrapSelection;
    int             alignment;
    qreal           fixedwidth;
    qreal           fixedheight;
    qreal           spacingX;
    qreal           spacingY;

  private:
    Q_DISABLE_COPY(UIGroup)
};

#endif // UIGROUP_H
