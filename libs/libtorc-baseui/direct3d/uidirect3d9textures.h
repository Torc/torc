#ifndef UIDIRECT3D9TEXTURES_H
#define UIDIRECT3D9TEXTURES_H

// Qt
#include <QSize>
#include <QVector>
#include <QImage>

// Torc
#include "torcbaseuiexport.h"
#include "../uiimage.h"
#include "d3dx9.h"
#include "uidirect3d9defs.h"

class TORC_BASEUI_PUBLIC D3D9Texture
{
  public:
    D3D9Texture(IDirect3DSurface9* Surface, IDirect3DTexture9* Texture, IDirect3DVertexBuffer9* VertexBuffer,
                const QSize &Size, const QSize &UsedSize);
    virtual ~D3D9Texture();

    IDirect3DSurface9*      m_surface;
    IDirect3DTexture9*      m_texture;
    IDirect3DVertexBuffer9* m_vertexBuffer;
    QSize                   m_size;
    QSize                   m_usedSize;
    quint64                 m_internalDataSize;
    bool                    m_fullVertices;
    bool                    m_verticesUpdated;
};

class TORC_BASEUI_PUBLIC UIDirect3D9Textures
{
  public:
    UIDirect3D9Textures();
    virtual ~UIDirect3D9Textures();

    bool         InitialiseTextures  (IDirect3D9 *Object, IDirect3DDevice9 *Device,
                                      unsigned int Adaptor, D3DFORMAT AdaptorFormat);
    D3D9Texture* CreateTexture       (IDirect3DDevice9 *Device, const QSize &Size);
    void         UpdateTexture       (IDirect3DDevice9 *Device, D3D9Texture *Texture, UIImage *Image);
    void         UpdateVertices      (D3D9Texture *Texture, QRectF *Dest, QSizeF *Size);
    void         DeleteTexture       (D3D9Texture *Texture);

  protected:
    QSize        GetTextureSize      (QSize Size);

  private:
    void         DeleteTexures       (void);

  protected:
    bool         m_texturesValid;
    bool         m_allowRects;
    D3DFORMAT    m_adaptorFormat;
    D3DFORMAT    m_surfaceFormat;
    D3DFORMAT    m_textureFormat;
    DWORD        m_defaultUsage;
    D3DPOOL      m_defaultPool;

    QVector<D3D9Texture*> m_textures;

    TORC_D3DXLOADSURFACEFROMMEMORY m_D3DXLoadSurface;
};

#endif // UIDIRECT3D9TEXTURES_H
