/* Class UIDirect3D9Textures
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2013
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Qt
#include <QString>

// Torc
#include "torclogging.h"
#include "uidirect3d9window.h"
#include "uidirect3d9textures.h"

// TODO just add d3dx9tex.h
#define D3DX_FILTER_NONE                 0x00000001
#define D3DX_FILTER_LINEAR               0x00000003

#define CHECK(MSG,ARG) \
    if (FAILED(ARG)) \
    LOG(VB_GENERAL, LOG_ERR, MSG);

static const QString FormatToString(D3DFORMAT Format)
{
    switch (Format)
    {
        case D3DFMT_A8:
            return "A8";
        case D3DFMT_A8R8G8B8:
            return "A8R8G8B8";
        case D3DFMT_X8R8G8B8:
            return "X8R8G8B8";
        case D3DFMT_A8B8G8R8:
            return "A8B8G8R8";
        case D3DFMT_X8B8G8R8:
            return "X8B8G8R8";
        case D3DFMT_R8G8B8:
            return "R8G8B8";
        default:
            break;
    }

    return QString().setNum((unsigned long)Format,16);
}

D3D9Texture::D3D9Texture(IDirect3DSurface9 *Surface, IDirect3DTexture9 *Texture, IDirect3DVertexBuffer9* VertexBuffer,
                         const QSize &Size, const QSize &UsedSize)
  : m_surface(Surface),
    m_texture(Texture),
    m_vertexBuffer(VertexBuffer),
    m_size(Size),
    m_usedSize(UsedSize)
{
    // FIXME
    m_internalDataSize = m_size.width() * m_size.height() * 4;
}

D3D9Texture::~D3D9Texture()
{
    if (m_surface)
        m_surface->Release();

    if (m_texture)
        m_texture->Release();

    if (m_vertexBuffer)
        m_vertexBuffer->Release();
}

UIDirect3D9Textures::UIDirect3D9Textures()
  : m_texturesValid(false),
    m_allowRects(false),
    m_adaptorFormat(D3DFMT_UNKNOWN),
    m_surfaceFormat(D3DFMT_UNKNOWN),
    m_textureFormat(D3DFMT_UNKNOWN),
    m_defaultUsage(0),
    m_defaultPool(D3DPOOL_DEFAULT),
    m_D3DXLoadSurface(NULL)
{
}

UIDirect3D9Textures::~UIDirect3D9Textures()
{
    DeleteTexures();
}

bool UIDirect3D9Textures::InitialiseTextures(IDirect3D9 *Object, IDirect3DDevice9 *Device,
                                             unsigned int Adaptor, D3DFORMAT AdaptorFormat)
{
    D3DCAPS9 capabilities;
    memset(&capabilities, 0, sizeof(capabilities));
    Object->GetDeviceCaps(Adaptor, D3DDEVTYPE_HAL, &capabilities);

    m_defaultPool   = D3DPOOL_DEFAULT;
    m_defaultUsage  = D3DUSAGE_DYNAMIC;//0;
    m_allowRects    = capabilities.TextureCaps & D3DPTEXTURECAPS_POW2;
    m_surfaceFormat = m_adaptorFormat;

    static const D3DFORMAT formats[] =
    {
        D3DFMT_A8R8G8B8,
        D3DFMT_A8B8G8R8,
        D3DFMT_X8R8G8B8,
        D3DFMT_X8B8G8R8,
        D3DFMT_R8G8B8
    };

    for (unsigned int i = 0; i < sizeof(formats) / sizeof(D3DFORMAT); ++i)
    {
        if (SUCCEEDED(Object->CheckDeviceType(Adaptor, D3DDEVTYPE_HAL,
                                              AdaptorFormat, formats[i], TRUE)))
        {
            m_surfaceFormat = formats[i];
            break;
        }
    }

    m_textureFormat = m_surfaceFormat;

    LOG(VB_GENERAL, LOG_INFO, QString("Adaptor format   : %1").arg(FormatToString(m_adaptorFormat)));
    LOG(VB_GENERAL, LOG_INFO, QString("Texture format   : %2").arg(FormatToString(m_textureFormat)));
    LOG(VB_GENERAL, LOG_INFO, QString("Max texture size : %1x%2")
        .arg(capabilities.MaxTextureWidth).arg(capabilities.MaxTextureHeight));

    m_D3DXLoadSurface = (TORC_D3DXLOADSURFACEFROMMEMORY)UIDirect3D9Window::GetD3DX9ProcAddress("D3DXLoadSurfaceFromMemory");

    m_texturesValid = m_D3DXLoadSurface;

    return m_texturesValid;
}

D3D9Texture* UIDirect3D9Textures::CreateTexture(IDirect3DDevice9 *Device, const QSize &Size)
{
    if (Device && m_texturesValid)
    {
        QSize actualsize = GetTextureSize(Size);

        IDirect3DSurface9 *d3dsurface = NULL;
        HRESULT result = Device->CreateOffscreenPlainSurface(actualsize.width(), actualsize.height(),
                                                             m_surfaceFormat, D3DPOOL_DEFAULT, &d3dsurface, NULL);

        if (d3dsurface &&SUCCEEDED(result))
        {
            IDirect3DTexture9 *d3dtexture = NULL;
            result = Device->CreateTexture(actualsize.width(), actualsize.height(), 1, D3DUSAGE_RENDERTARGET,
                                           m_textureFormat, D3DPOOL_DEFAULT, &d3dtexture, NULL);
            if (d3dtexture && SUCCEEDED(result))
            {
                IDirect3DVertexBuffer9* d3dvertexbuffer = NULL;
                result = Device->CreateVertexBuffer(20 * sizeof(float), D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,
                                                    0, D3DPOOL_DEFAULT, &d3dvertexbuffer, NULL);

                if (d3dvertexbuffer && SUCCEEDED(result))
                {
                    D3D9Texture* texture = new D3D9Texture(d3dsurface, d3dtexture, d3dvertexbuffer, actualsize, Size);
                    m_textures.push_back(texture);
                    return texture;
                }

                if (d3dvertexbuffer)
                    d3dvertexbuffer->Release();
            }

            if (d3dtexture)
                d3dtexture->Release();
         }

        if (d3dsurface)
            d3dsurface->Release();
    }

    LOG(VB_GENERAL, LOG_ERR, "Failed to create texture");
    return NULL;
}

void UIDirect3D9Textures::UpdateTexture(IDirect3DDevice9 *Device, D3D9Texture *Texture, UIImage *Image)
{
    if (Device && Texture && Image && m_texturesValid)
    {
        if (Texture->m_surface && Texture->m_texture)
        {
            RECT source;
            source.left   = 0;
            source.top    = 0;
            source.right  = Image->size().width();
            source.bottom = Image->size().height();

            HRESULT result = m_D3DXLoadSurface(Texture->m_surface, NULL, &source, (LPCVOID)Image->bits(),
                                               D3DFMT_A8R8G8B8, Image->bytesPerLine(), NULL, &source,
                                               D3DX_FILTER_LINEAR, 0);

            if (SUCCEEDED(result))
            {
                IDirect3DSurface9 *d3ddest;
                result = Texture->m_texture->GetSurfaceLevel(0, &d3ddest);
                if (SUCCEEDED(result))
                {
                    Device->StretchRect(Texture->m_surface, NULL, d3ddest, NULL, D3DTEXF_POINT);
                    d3ddest->Release();
                }
                else
                {
                    LOG(VB_GENERAL, LOG_ERR, "Failed to update texture");
                }
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, "Failed to update surface");
            }
        }
    }
}

void UIDirect3D9Textures::UpdateVertices(D3D9Texture *Texture, QRectF *Dest, QSizeF *Size)
{
    if (Texture && Dest && Size && Texture->m_vertexBuffer)
    {
        float *v;
        if (SUCCEEDED(Texture->m_vertexBuffer->Lock(0, 0, (void**)(&v), D3DLOCK_DISCARD)))
        {
            float left    = 0.0;//Dest->left();
            float right   = left + Dest->width();
            float top     = 0.0;//Dest->top();
            float bottom  = top + Dest->height();
            float tright  = Size->width();
            float tbottom = Size->height();

            tright  /= Texture->m_size.width();
            tbottom /= Texture->m_size.height();

            v[0] = left;
            v[1] = top;
            v[2] = 0.1;
            v[3] = 0.0;
            v[4] = 0.0;

            v[5] = left;
            v[6] = bottom;
            v[7] = 0.1;
            v[8] = 0.0;
            v[9] = tbottom;

            v[10] = right;
            v[11] = top;
            v[12] = 0.1;
            v[13] = tright;
            v[14] = 0.0;

            v[15] = right;
            v[16] = bottom;
            v[17] = 0.1;
            v[18] = tright;
            v[19] = tbottom;

            Texture->m_vertexBuffer->Unlock();
        }
    }
}

void UIDirect3D9Textures::DeleteTexture(D3D9Texture *Texture)
{
    if (m_texturesValid)
    {
        QVector<D3D9Texture*>::iterator it = m_textures.begin();
        for ( ; it != m_textures.end(); ++it)
        {
            if (*it == Texture)
            {
                m_textures.erase(it);
                delete Texture;
                break;
            }
        }
    }
}

void UIDirect3D9Textures::DeleteTexures(void)
{
    if (m_texturesValid)
    {
        foreach(D3D9Texture* texture, m_textures)
            delete texture;
        m_textures.clear();
    }
}

QSize UIDirect3D9Textures::GetTextureSize(QSize Size)
{
    if (m_allowRects)
        return Size;

    int w = 64;
    int h = 64;

    while (w < Size.width())
        w *= 2;

    while (h < Size.height())
        h *= 2;

    return QSize(w, h);
}
