#ifndef UIWIDGET_H
#define UIWIDGET_H

// Qt
#include <QtScript>
#include <QMetaType>
#include <QColor>
#include <QHash>
#include <QObject>
#include <QRectF>

// Torc
#include "torcreferencecounted.h"
#include "torcbaseuiexport.h"

class QScriptEngine;
class QDomElement;
class UIAnimation;
class UIWindow;
class UIEffect;
class UIWidget;
class UIFont;
class UIConnection;

class TORC_BASEUI_PUBLIC UIWidget : public QObject, public TorcReferenceCounter
{
    Q_OBJECT

    friend class UITheme;
    friend class TenfootTheme;
    friend class UIShapePath;
    friend class UIGroup;
    friend class UIMessenger;
    friend class UITextEditor;

  public:
    enum WidgetFlags
    {
        WidgetFlagNone       = 0x00,
        WidgetFlagTemplate   = 0x01,
        WidgetFlagFocusable  = 0x02,
        WidgetFlagDecoration = 0x04
    };

    static  int     kUIWidgetType;
    static  int     RegisterWidgetType(void);

  public:
    UIWidget        (UIWidget *Root, UIWidget* Parent, const QString &Name, int Flags);
    virtual         ~UIWidget();

    virtual bool    Refresh             (quint64 TimeNow);
    virtual bool    Draw                (quint64 TimeNow, UIWindow* Window, qreal XOffset, qreal YOffset);
    bool            Initialise          (QDomElement *Element, const QString &Position);
    virtual bool    Finalise            (void);
    void            AddFont             (const QString &FontName, UIFont* Font);
    UIFont*         GetFont             (const QString &FontName);
    UIWidget*       FindWidget          (const QString &Name);
    virtual bool    AutoSelectFocusWidget (int Index);
    UIWidget*       GetFocusWidget      (void);
    void            SetLastChildWithFocus (UIWidget *Widget);
    UIWidget*       GetLastChildWithFocus (void);
    bool            HasChildWithFocus   (void);
    void            IncreaseFocusableChildCount (void);
    void            DecreaseFocusableChildCount (void);
    int             GetDirection        (void);
    void            SetDirection        (int Direction);
    virtual bool    HandleAction        (int       Action);
    virtual void    CopyFrom            (UIWidget *Other);
    UIWidget*       GetRootWidget       (void);
    int             Type                (void);
    void            SetAsTemplate       (bool Template);
    bool            IsTemplate          (void);
    bool            IsDecoration        (void);

    // Theme parsing
    static bool     ParseWidget         (UIWidget *Root, UIWidget *Parent, QDomElement *Element);
    static bool     ParseFont           (UIWidget *Root, QDomElement *Element);
    static QString  GetText             (QDomElement *Element);
    static QColor   GetQColor           (const QString &Color);
    static QRectF   GetRect             (QDomElement  *Element);
    static QRectF   GetRect             (const QString &Rect);
    static QSizeF   GetSizeF            (const QString &Size);
    static QPoint   GetPoint            (const QString &Point);
    static QPointF  GetPointF           (const QString &Point);
    static bool     GetBool             (QDomElement *Element);
    static bool     GetBool             (const QString &Bool);
    static int      GetAlignment        (const QString &Alignment);
    QRectF          GetScaledRect       (const QString &Rect);
    QRectF          ScaleRect           (const QRectF  &Rect);
    QPointF         GetScaledPointF     (const QString &Point);
    QPointF         ScalePointF         (const QPointF &Point);
    QSizeF          GetScaledSizeF      (const QString &Size);
    QSizeF          ScaleSizeF          (const QSizeF  &Size);

    // Scriptable properties
    Q_PROPERTY      (bool    m_focusable       READ IsFocusable)
    Q_PROPERTY      (bool    m_enabled         READ IsEnabled           WRITE SetEnabled)
    Q_PROPERTY      (bool    m_visible         READ IsVisible           WRITE SetVisible)
    Q_PROPERTY      (bool    m_active          READ IsActive            WRITE SetActive)
    Q_PROPERTY      (bool    m_selected        READ IsSelected          WRITE SetSelected)
    Q_PROPERTY      (bool    m_positionChanged READ IsPositionChanged   WRITE SetPositionChanged)
    Q_PROPERTY      (qreal   alpha             READ GetAlpha            WRITE SetAlpha)
    Q_PROPERTY      (qreal   color             READ GetColor            WRITE SetColor)
    Q_PROPERTY      (qreal   zoom              READ GetZoom             WRITE SetZoom)
    Q_PROPERTY      (qreal   horizontalzoom    READ GetHorizontalZoom   WRITE SetHorizontalZoom)
    Q_PROPERTY      (qreal   verticalzoom      READ GetVerticalZoom     WRITE SetVerticalZoom)
    Q_PROPERTY      (qreal   rotation          READ GetRotation         WRITE SetRotation)
    Q_PROPERTY      (QPointF position          READ GetPosition         WRITE SetPosition)
    Q_PROPERTY      (QSizeF  size              READ GetSize             WRITE SetSize)
    Q_PROPERTY      (int     centre            READ GetCentre           WRITE SetCentre)
    Q_PROPERTY      (qreal   hreflection       READ GetHorizontalReflection WRITE SetHorizontalReflection)
    Q_PROPERTY      (qreal   vreflection       READ GetVerticalReflection   WRITE SetVerticalReflection)

  public slots:
    bool            IsFocusable         (void);
    bool            IsButton            (void);
    bool            IsEnabled           (void);
    bool            IsVisible           (void);
    bool            IsActive            (void);
    bool            IsSelected          (void);
    bool            IsPositionChanged   (void);
    qreal           GetAlpha            (void);
    qreal           GetColor            (void);
    qreal           GetZoom             (void);
    qreal           GetHorizontalZoom   (void);
    qreal           GetVerticalZoom     (void);
    qreal           GetRotation         (void);
    QPointF         GetPosition         (void);
    QSizeF          GetSize             (void);
    int             GetCentre           (void);
    qreal           GetHorizontalReflection (void);
    qreal           GetVerticalReflection   (void);

    void            SetEnabled          (bool    Enabled);
    void            SetVisible          (bool    Visible);
    void            SetActive           (bool    Active);
    void            SetSelected         (bool    Selected);
    void            SetPositionChanged  (bool    Changed);
    void            SetAlpha            (qreal   Alpha);
    void            SetColor            (qreal   Color);
    void            SetZoom             (qreal   Zoom);
    void            SetHorizontalZoom   (qreal   HorizontalZoom);
    void            SetVerticalZoom     (qreal   VerticalZoom);
    void            SetRotation         (qreal   Rotate);
    void            SetCentre           (int     Centre);
    void            SetHorizontalReflection (qreal Reflect);
    void            SetVerticalReflection   (qreal Reflect);
    void            SetHorizontalReflecting (bool  Reflecting);
    void            SetVerticalReflecting   (bool  Reflecting);
    void            SetPosition         (QPointF Position);
    void            SetPosition         (qreal   Left, qreal Top);
    void            SetSize             (QSizeF  Size);
    void            SetSize             (qreal   Width, qreal Height);
    void            SetClippingRect     (int     Left, int Top, int Width, int Height);

    void            Close               (void);
    void            Enable              (void);
    void            Disable             (void);
    void            Show                (void);
    void            Hide                (void);
    void            Activate            (void);
    void            Deactivate          (void);
    void            Select              (void);
    void            Deselect            (void);

    void            HideAll             (void);
    void            ShowAll             (void);
    bool            SetAsRootWidget     (void);
    bool            SetRootWidget       (UIWidget *Widget);
    bool            SetRootWidget       (const QString &Widget);

  signals:
    void            Finalised           (void);
    void            Closed              (void);
    void            Enabled             (void);
    void            Disabled            (void);
    void            Shown               (void);
    void            Hidden              (void);
    void            Activated           (void);
    void            Deactivated         (void);
    void            Selected            (void);
    void            Deselected          (void);

  protected slots:
    void            ScriptException     (void);

  protected:
    virtual bool    DrawSelf            (UIWindow* Window, qreal XOffset, qreal YOffset);
    virtual bool    InitialisePriv      (QDomElement *Element);
    virtual void    CreateCopy          (UIWidget *Parent);
    QString         GetDerivedWidgetName(const QString &NewParentName);
    void            AddChild            (UIWidget *Widget);
    void            RemoveChild         (UIWidget *Widget);
    void            RegisterWidget      (UIWidget *Widget);
    void            DeregisterWidget    (UIWidget *Widget);
    UIWidget*       FindChildByName     (const QString &Name, bool Recursive = false);

    bool            Connect             (const QString &Sender, const QString &Signal,
                                         const QString &Receiver, const QString &Slot);
    virtual void    AutoConnect         (void);

    QScriptEngine*  GetScriptEngine     (void);
    QScriptEngine*  PreValidateProperty (const QString &Property, bool IsObject);
    void            AddScriptProperty   (const QString &Property, const QString &Value);
    void            AddScriptObject     (QObject *Object);
    void            RemoveScriptProperty(const QString &Property);

    void            Debug               (int Depth = 0);
    void            SetScaledPosition   (const QPointF &Position);
    qreal           GetXScale           (void);
    qreal           GetYScale           (void);

  protected:
    int             m_type;
    UIWidget       *m_rootParent;
    UIWidget       *m_parent;
    UIWidget       *m_lastChildWithFocus;
    bool            m_focusable;
    bool            m_childHasFocus;
    int             m_focusableChildCount;
    bool            m_template;
    bool            m_decoration;
    QRectF          m_unscaledRect;

    // resources
    QList<UIWidget*>         m_children;
    QList<UIConnection*>     m_connections;
    QList<QString>           m_scriptProperties;

    // state
    bool            m_enabled;
    bool            m_visible;
    bool            m_active;
    bool            m_selected;
    UIEffect       *m_effect;
    UIEffect       *m_secondaryEffect;
    QRectF          m_scaledRect;
    bool            m_positionChanged;
    qreal           m_scaleX;
    qreal           m_scaleY;
    bool            m_clipping;
    QRect           m_clipRect;

    // these are proxies for members of UIEffect and double up as defaults
    qreal           alpha;
    qreal           color;
    qreal           zoom;
    qreal           verticalzoom;
    qreal           horizontalzoom;
    qreal           rotation;
    int             centre;
    bool            hreflecting;
    qreal           hreflection;
    bool            vreflecting;
    qreal           vreflection;
    QPointF         position;
    QSizeF          size;

  private:
    void            SetFocusWidget       (UIWidget *Widget);
    void            SetHasChildWithFocus (bool HasFocus);

  private:
    // root widget only
    QHash<QString,UIWidget*> m_widgetNames;
    QHash<QString,UIFont*>   m_fonts;
    QScriptEngine           *m_engine;
    UIWidget                *m_globalFocusWidget;
    int                      m_direction;
};

class TORC_BASEUI_PUBLIC WidgetFactory
{
  public:
    WidgetFactory();
    virtual ~WidgetFactory();
    static WidgetFactory* GetWidgetFactory    (void);
    WidgetFactory*        NextFactory         (void) const;
    virtual void          RegisterConstructor (QScriptEngine *Engine) = 0;
    virtual UIWidget*     Create              (const QString &Type, UIWidget *Root,
                                               UIWidget *Parent, const QString &Name,
                                               int Flags) = 0;

  protected:
    static WidgetFactory* gWidgetFactory;
    WidgetFactory*        nextWidgetFactory;
};

template <class T> QScriptValue WidgetConstructor(QScriptContext *context, QScriptEngine *engine)
{
    if (context->argumentCount() < 2)
    {
        context->throwError(QScriptContext::TypeError, "UIWidgetConstructor needs at least 2 arguments.");
        return engine->undefinedValue();
    }

    QScriptValue parentvalue = context->argument(0);
    QString name             = context->argument(1).toString();

    UIWidget* parentwidget = qscriptvalue_cast<UIWidget*>(parentvalue);

    if (!parentwidget)
    {
        context->throwError(QScriptContext::TypeError, "Failed to cast script value to UIWidget.");
        return engine->undefinedValue();
    }

    UIWidget* rootwidget   = parentwidget->GetRootWidget();
    if (!rootwidget)
    {
        context->throwError(QScriptContext::TypeError, "Failed to get root widget.");
        return engine->undefinedValue();
    }

    UIWidget* base = NULL;
    if (context->argumentCount() >= 3)
    {
        QString from = context->argument(2).toString();
        UIWidget* base = rootwidget->FindWidget(from);
        if (!base)
        {
            context->throwError(QScriptContext::TypeError, "Failed to find base widget.");
            return engine->undefinedValue();
        }
    }

    T* widget = new T(rootwidget, parentwidget, name, UIWidget::WidgetFlagNone);

    if (base)
        widget->CopyFrom(base);

    return engine->newQObject(widget);
}

Q_DECLARE_METATYPE(UIWidget*);

#endif // UIWIDGET_H
