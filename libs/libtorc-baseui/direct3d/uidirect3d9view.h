#ifndef UIDIRECT3D9VIEW_H
#define UIDIRECT3D9VIEW_H

// Torc
#include "torcbaseuiexport.h"
#include "uidirect3d9defs.h"

#define MAX_STACK_DEPTH 10

class UIEffect;

class TORC_BASEUI_PUBLIC UID3DMatrix
{
  public:
    UID3DMatrix();

    D3DXMATRIX m;
    float      alpha;
    float      color;
};

class TORC_BASEUI_PUBLIC UIDirect3D9View
{
    enum Projection
    {
        ViewPortOnly,
        Parallel,
        Perspective
    };

  public:
    UIDirect3D9View();
    virtual ~UIDirect3D9View();

    void  SetViewPort        (IDirect3DDevice9* Device, const QRect &Rect, Projection Type);
    QRect GetViewPort        (void);
    void  SetProjection      (Projection Type);

  protected:
    bool  InitialiseView     (IDirect3DDevice9 *Device, const QRect &Rect);
    bool  PushTransformation (const UIEffect *Effect, const QRectF *Dest);
    void  PopTransformation  (void);

  protected:
    bool         m_viewValid;
    QRect        m_viewport;
    D3DXMATRIX  *m_currentProjection;
    D3DXMATRIX   m_parallel;
    D3DXMATRIX   m_perspective;
    int          m_currentTransformIndex;
    UID3DMatrix  m_transforms[MAX_STACK_DEPTH];

    TORC_D3DXMATRIXMULTIPLY               m_D3DXMatrixMultiply;
    TORC_D3DXMATRIXPERSPECTIVEOFFCENTERLH m_D3DXMatrixPerspectiveOffCenterLH;
    TORC_D3DXMATRIXORTHOOFFCENTERLH       m_D3DXMatrixOrthoOffCenterLH;
    TORC_D3DXMATRIXTRANSLATION            m_D3DXMatrixTranslation;
    TORC_D3DXMATRIXREFLECT                m_D3DXMatrixReflect;
    TORC_D3DXMATRIXROTATIONZ              m_D3DXMatrixRotationZ;
    TORC_D3DXMATRIXSCALING                m_D3DXMatrixScaling;
};

#endif // UIDIRECT3D9VIEW_H
