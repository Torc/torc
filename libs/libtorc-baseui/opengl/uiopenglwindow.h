#ifndef UIOPENGLWINDOW_H
#define UIOPENGLWINDOW_H

// Qt
#include <QVariantMap>
#include <QHash>
#include <QList>

// Torc
#include "http/torchttpservice.h"
#include "../torcbaseuiexport.h"
#include "uiopengldefs.h"
#include "../uiwindow.h"
#include "../uidisplay.h"
#include "../uiimagetracker.h"
#include "uiopenglshaders.h"
#include "uiopenglfence.h"
#include "uiopengltextures.h"
#include "uiopenglframebuffers.h"
#include "uiopenglview.h"
#include "../uiperformance.h"
#include "uiactions.h"

class UIImage;
class UITimer;
class UIShapePath;

typedef void (*QGLFunctionPointer)();

class TORC_BASEUI_PUBLIC UIOpenGLWindow
  : public QGLWidget,
    public UIDisplay,
    public UIWindow,
    public UIImageTracker,
    public UIOpenGLShaders,
    public UIOpenGLFence,
    public UIOpenGLFramebuffers,
    public UIPerformance,
    public UIActions,
    public TorcHTTPService
{
    Q_OBJECT

  public:
    // UIOpenGLWindow
    static UIOpenGLWindow* Create(void);
    virtual ~UIOpenGLWindow();

    static QGLFunctionPointer GetProcAddress (const QString &Proc);

    // UIWindow
    QSize       GetSize           (void);
    void        SetRefreshRate    (double Rate, int ModeIndex = -1);

    void        DrawImage         (UIEffect *Effect,
                                   QRectF   *Dest,
                                   bool     &PositionChanged,
                                   UIImage  *Image);
    UIImage*    DrawText          (UIEffect *Effect,
                                   QRectF   *Dest,
                                   bool     &PositionChanged,
                                   const QString &Text,
                                   UIFont   *Font,
                                   int       Flags,
                                   int       Blur,
                                   UIImage  *Fallback = NULL);
    void        DrawShape         (UIEffect *Effect,
                                   QRectF *Dest, bool &PositionChanged,
                                   UIShapePath *Path);
    bool        PushEffect        (const UIEffect *Effect, const QRectF *Dest);
    void        PopEffect         (void);
    void        PushClip          (const QRect &Rect);
    void        PopClip           (void);

    void        SetBlend          (bool Enable);
    void        DrawTexture       (GLTexture *Texture, QRectF *Dest, QSizeF *Size, uint Shader);
    void        DrawTexture       (GLTexture *Texture, QRectF *Dest, QSizeF *Size, bool &PositionChanged, bool Blend = true);

  public slots:
    // UIWindow
    void        MainLoop          (void);

    QVariantMap GetDisplayDetails (void);
    QVariantMap GetThemeDetails   (void);

  protected:
    // UIOpenGLWindow
    UIOpenGLWindow(const QGLFormat &Format, GLType Type = kGLOpenGL2);

    // QWidget
    bool       eventFilter        (QObject     *Object, QEvent *Event);
    void       customEvent        (QEvent      *Event);
    void       closeEvent         (QCloseEvent *Event);
    bool       event              (QEvent      *Event);

  private:    
    // UIOpenGLWindow
    Q_DISABLE_COPY(UIOpenGLWindow);

    void       InitialiseWindow   (void);
    GLTexture *AllocateTexture    (UIImage *Image);
    void       ReleaseTexture     (UIImage *Image);
    void       ReleaseAllTextures (void);
    void       SetBackground      (quint8 Red, quint8 Green, quint8 Blue, quint8 Alpha);

  private:
    UITimer                      *m_timer;
    QHash<UIImage*,GLTexture*>    m_textureHash;
    QList<UIImage*>               m_textureExpireList;

    GLType                        m_openGLType;
    QString                       m_extensions;
    bool                          m_blend;
    quint32                       m_backgroundColour;
};

#endif // UIOPENGLWINDOW_H
