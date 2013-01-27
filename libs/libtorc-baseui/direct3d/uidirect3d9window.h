#ifndef UIDIRECT3D9WINDOW_H
#define UIDIRECT3D9WINDOW_H

// Qt
#include <QWidget>
#include <QVariant>

// Torc
#include "torclocaldefs.h"
#include "torccompat.h"
#include "http/torchttpservice.h"
#include "../torcbaseuiexport.h"
#include "../uidisplay.h"
#include "../uiactions.h"
#include "../uiimagetracker.h"
#include "../uiperformance.h"
#include "../uiwindow.h"

#include "d3d9.h"
#include "uidirect3d9view.h"
#include "uidirect3d9textures.h"
#include "uidirect3d9shaders.h"

class UITimer;
class QPaintEngine;

class TORC_BASEUI_PUBLIC UIDirect3D9Window
  : public QWidget,
    public UIDirect3D9View,
    public UIDirect3D9Textures,
    public UIDirect3D9Shaders,
    public UIDisplay,
    public UIWindow,
    public UIImageTracker,
    public UIPerformance,
    public UIActions,
    public TorcHTTPService
{
    Q_OBJECT

  public:
    static   UIDirect3D9Window*   Create (void);
    virtual ~UIDirect3D9Window();

    static  void* GetD3DX9ProcAddress (const QString &Proc);

    QSize       GetSize             (void);
    void        SetRefreshRate      (double Rate, int ModeIndex = -1);
    void        SetRenderTarget     (D3D9Texture* Texture);
    void        DrawTexture         (D3D9Texture* Texture, QRectF *Dest, QSizeF *Size, bool &PositionChanged);
    void        DrawTexture         (D3D9Shader* Shader, D3D9Texture* Texture, QRectF *Dest, QSizeF *Size, bool &PositionChanged);
    void        DrawImage           (UIEffect *Effect, QRectF *Dest, bool &PositionChanged, UIImage *Image);
    UIImage*    DrawText            (UIEffect *Effect, QRectF *Dest, bool &PositionChanged,
                                     const QString &Text, UIFont *Font, int Flags, int Blur,
                                     UIImage  *Fallback = NULL);
    void        DrawShape           (UIEffect *Effect, QRectF *Dest, bool &PositionChanged, UIShapePath *Path);
    bool        PushEffect          (const UIEffect *Effect, const QRectF *Dest);
    void        PopEffect           (void);
    void        PushClip            (const QRect &Rect);
    void        PopClip             (void);
    void        SetBlend            (bool Enable);

    QPaintEngine* paintEngine() const;
    IDirect3DDevice9* GetDevice() const;

  public slots:
    void         MainLoop           (void);
    QVariantMap  GetDisplayDetails  (void);
    QVariantMap  GetThemeDetails    (void);

  protected:
    UIDirect3D9Window();
    bool         Initialise         (void);

    // QWidget
    void         customEvent        (QEvent      *Event);
    void         closeEvent         (QCloseEvent *Event);
    bool         event              (QEvent      *Event);

    bool         InitialiseWindow   (void);
    void         Destroy            (void);
    D3D9Texture* AllocateTexture    (UIImage *Image);
    void         ReleaseTexture     (UIImage *Image);
    void         ReleaseAllTextures (void);

  private:
    UITimer                     *m_timer;
    IDirect3D9                  *m_d3dObject;
    IDirect3DDevice9            *m_d3dDevice;
    IDirect3DSurface9           *m_defaultRenderTarget;
    IDirect3DSurface9           *m_currentRenderTarget;
    QHash<UIImage*,D3D9Texture*> m_textureHash;
    QList<UIImage*>              m_textureExpireList;
    bool                         m_blend;
    UIActions                    m_actions;

  private:
    Q_DISABLE_COPY(UIDirect3D9Window);
};

#endif // UIDIRECT3D9WINDOW_H
