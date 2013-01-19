#ifndef UIWINDOW_H
#define UIWINDOW_H

// Torc
#include "torcbaseuiexport.h"
#include "uifont.h"

class QMutex;
class UIEffect;
class UIImage;
class UITheme;
class UIShapePath;

class TORC_BASEUI_PUBLIC UIWindow
{
  public:
    UIWindow();
    virtual ~UIWindow();

    virtual QSize    GetSize           (void) = 0;
    virtual void     SetRefreshRate    (double Rate, int ModeIndex = -1) = 0;

    virtual void     DrawImage         (UIEffect *Effect,
                                        QRectF   *Dest,
                                        bool     &PositionChanged,
                                        UIImage  *Image) = 0;

    virtual UIImage* DrawText          (UIEffect *Effect,
                                        QRectF   *Dest,
                                        bool     &PositionChanged,
                                        const QString &Text,
                                        UIFont   *Font,
                                        int       Flags,
                                        int       Blur,
                                        UIImage  *Fallback = NULL) = 0;

    virtual void     DrawShape         (UIEffect *Effect,
                                        QRectF *Dest, bool &PositionChanged,
                                        UIShapePath *Path) = 0;

    virtual bool     PushEffect        (const UIEffect *Effect, const QRectF *Dest) = 0;
    virtual void     PopEffect         (void) = 0;
    virtual void     PushClip          (const QRect &Rect) = 0;
    virtual void     PopClip           (void) = 0;

    static UIWindow* Create            (void);

    void             ThemeReady        (UITheme *Theme);
    bool             GetStudioLevels   (void);
    void             SetStudioLevels   (bool Value);

  protected:
    void             CheckForNewTheme  (void);

  protected:
    UITheme         *m_theme;
    UITheme         *m_newTheme;
    QAtomicInt       m_haveNewTheme;
    QMutex          *m_newThemeLock;
    int              m_mainTimer;
    bool             m_studioLevels;
};

#endif // UIWINDOW_H
