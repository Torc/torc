/* Class UIDirect3D9View
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

// Torc
#include "torclogging.h"
#include "../uieffect.h"
#include "uidirect3d9window.h"
#include "uidirect3d9view.h"

UID3DMatrix::UID3DMatrix()
  : alpha(1.0),
    color(1.0)
{
    D3DXMatrixIdentity(&m);
}

UIDirect3D9View::UIDirect3D9View()
  : m_viewValid(false),
    m_currentTransformIndex(0),
    m_currentClipIndex(0),
    m_D3DXMatrixMultiply(NULL),
    m_D3DXMatrixPerspectiveOffCenterLH(NULL),
    m_D3DXMatrixOrthoOffCenterLH(NULL),
    m_D3DXMatrixTranslation(NULL),
    m_D3DXMatrixReflect(NULL),
    m_D3DXMatrixRotationZ(NULL),
    m_D3DXMatrixScaling(NULL)
{
    m_currentProjection = &m_parallel;
    m_transforms[0] = UID3DMatrix();
}

UIDirect3D9View::~UIDirect3D9View()
{
}

bool UIDirect3D9View::InitialiseView(IDirect3DDevice9 *Device, const QRect &Rect)
{
    m_D3DXMatrixMultiply               = (TORC_D3DXMATRIXMULTIPLY)UIDirect3D9Window::GetD3DX9ProcAddress("D3DXMatrixMultiply");
    m_D3DXMatrixPerspectiveOffCenterLH = (TORC_D3DXMATRIXPERSPECTIVEOFFCENTERLH)UIDirect3D9Window::GetD3DX9ProcAddress("D3DXMatrixPerspectiveOffCenterLH");
    m_D3DXMatrixOrthoOffCenterLH       = (TORC_D3DXMATRIXORTHOOFFCENTERLH)UIDirect3D9Window::GetD3DX9ProcAddress("D3DXMatrixOrthoOffCenterLH");
    m_D3DXMatrixTranslation            = (TORC_D3DXMATRIXTRANSLATION)UIDirect3D9Window::GetD3DX9ProcAddress("D3DXMatrixTranslation");
    m_D3DXMatrixReflect                = (TORC_D3DXMATRIXREFLECT)UIDirect3D9Window::GetD3DX9ProcAddress("D3DXMatrixReflect");
    m_D3DXMatrixRotationZ              = (TORC_D3DXMATRIXROTATIONZ)UIDirect3D9Window::GetD3DX9ProcAddress("D3DXMatrixRotationZ");
    m_D3DXMatrixScaling                = (TORC_D3DXMATRIXSCALING)UIDirect3D9Window::GetD3DX9ProcAddress("D3DXMatrixScaling");

    m_viewValid = m_D3DXMatrixMultiply && m_D3DXMatrixOrthoOffCenterLH && m_D3DXMatrixPerspectiveOffCenterLH &&
                  m_D3DXMatrixTranslation && m_D3DXMatrixReflect && m_D3DXMatrixRotationZ && m_D3DXMatrixScaling;

    SetViewPort(Device, Rect, Parallel);
    return m_viewValid;
}

void UIDirect3D9View::SetViewPort(IDirect3DDevice9 *Device, const QRect &Rect, Projection Type)
{
    if (m_viewValid && Device)
    {
        if (Rect == m_viewport)
            return;

        m_viewport = Rect;

        D3DVIEWPORT9 viewport;
        viewport.X      = m_viewport.left();
        viewport.Y      = m_viewport.top();
        viewport.Width  = m_viewport.width();
        viewport.Height = m_viewport.height();
        viewport.MinZ   = 0.0f;
        viewport.MaxZ   = 1.0f;

        Device->SetViewport(&viewport);

        SetProjection(Type);
    }
}

void UIDirect3D9View::SetProjection(Projection Type)
{
    float left   = m_viewport.left();
    float top    = m_viewport.top();
    float right  = left + m_viewport.width();
    float bottom = top + m_viewport.height();

    m_currentProjection = Type == Perspective ? &m_perspective : &m_parallel;

    m_D3DXMatrixPerspectiveOffCenterLH(&m_perspective, left, right, bottom, top, 0.1f, 100.0f);
    m_D3DXMatrixOrthoOffCenterLH(&m_parallel, left, right, bottom, top, 0.1f, 100.0f);
}

QRect UIDirect3D9View::GetViewPort(void)
{
    return m_viewport;
}

bool UIDirect3D9View::PushTransformation(const UIEffect *Effect, const QRectF *Dest)
{
    if (!Effect || !Dest || !m_viewValid)
        return false;

    if (m_currentTransformIndex >= MAX_STACK_DEPTH)
    {
        LOG(VB_GENERAL, LOG_INFO, "Max stack depth exceeded");
        return false;
    }

    D3DXMATRIX translate;
    UID3DMatrix matrix;
    matrix.alpha = m_transforms[m_currentTransformIndex].alpha * Effect->m_alpha;
    matrix.color = m_transforms[m_currentTransformIndex].color * Effect->m_color;

    if (Effect->m_hReflecting)
    {
        D3DXMATRIX reflect;
        D3DXPLANE  plane(0.0, 1.0, 0.0, 0.0);
        m_D3DXMatrixMultiply(&matrix.m, &matrix.m, m_D3DXMatrixTranslation(&translate, 0.0, -Effect->m_hReflection, 0.0));
        m_D3DXMatrixMultiply(&matrix.m, &matrix.m, m_D3DXMatrixReflect(&reflect, &plane));
        m_D3DXMatrixMultiply(&matrix.m, &matrix.m, m_D3DXMatrixTranslation(&translate, 0.0,  Effect->m_hReflection, 0.0));
    }
    else if (Effect->m_vReflecting)
    {
        D3DXMATRIX reflect;
        D3DXPLANE plane(1.0, 0.0, 0.0, 0.0);
        m_D3DXMatrixMultiply(&matrix.m, &matrix.m, m_D3DXMatrixTranslation(&translate, -Effect->m_vReflection, 0.0, 0.0));
        m_D3DXMatrixMultiply(&matrix.m, &matrix.m, m_D3DXMatrixReflect(&reflect, &plane));
        m_D3DXMatrixMultiply(&matrix.m, &matrix.m,  m_D3DXMatrixTranslation(&translate,  Effect->m_vReflection, 0.0, 0.0));
    }

    if (Effect->m_hZoom != 1.0 || Effect->m_vZoom != 1.0 || Effect->m_rotation != 0.0)
    {
        QPointF center = Effect->GetCentre(Dest);
        D3DXMATRIX rotate;
        D3DXMATRIX scale;
        m_D3DXMatrixMultiply(&matrix.m, &matrix.m, m_D3DXMatrixTranslation(&translate, -center.x(), -center.y(), 0.0));
        m_D3DXMatrixMultiply(&matrix.m, &matrix.m, m_D3DXMatrixRotationZ(&rotate, Effect->m_rotation * (M_PI / 180.0)));
        m_D3DXMatrixMultiply(&matrix.m, &matrix.m, m_D3DXMatrixScaling(&scale, Effect->m_hZoom, Effect->m_vZoom, 1.0));
        m_D3DXMatrixMultiply(&matrix.m, &matrix.m, m_D3DXMatrixTranslation(&translate, center.x(), center.y(), 0.0));
    }

    m_D3DXMatrixMultiply(&matrix.m, &matrix.m, m_D3DXMatrixTranslation(&translate, Dest->left(), Dest->top(), 0.0));
    m_D3DXMatrixMultiply(&matrix.m, &matrix.m, &m_transforms[m_currentTransformIndex].m);
    m_transforms[++m_currentTransformIndex] = matrix;

    return true;
}

void UIDirect3D9View::PopTransformation(void)
{
    if (m_currentTransformIndex > 0)
        m_currentTransformIndex--;
    else
        LOG(VB_GENERAL, LOG_ERR, "Mismatched calls to Push/PopTransformation");
}

void UIDirect3D9View::PushClipRect(IDirect3DDevice9 *Device, const QRect &Rect)
{
    if (m_currentClipIndex >= MAX_STACK_DEPTH)
    {
        LOG(VB_GENERAL, LOG_ERR, "Max clip depth reached");
        return;
    }

    if (++m_currentClipIndex == 1 && Device)
        Device->SetRenderState(D3DRS_SCISSORTESTENABLE, 1);

    m_clipRects[m_currentClipIndex] = Rect;

    if (Device)
    {
        RECT clip;
        clip.left   = Rect.left();
        clip.right  = clip.left + Rect.width();
        clip.top    = Rect.top();
        clip.bottom = clip.top + Rect.height();
        Device->SetScissorRect(&clip);
    }
}

void UIDirect3D9View::PopClipRect(IDirect3DDevice9 *Device)
{
    if (m_currentClipIndex > 0)
    {
        m_currentClipIndex--;

        if (m_currentClipIndex == 0 && Device)
        {
            Device->SetRenderState(D3DRS_SCISSORTESTENABLE, 0);
        }
        else if (Device)
        {
            RECT clip;
            clip.left   = m_clipRects[m_currentClipIndex].left();
            clip.right  = clip.left + m_clipRects[m_currentClipIndex].width();
            clip.top    = m_clipRects[m_currentClipIndex].top();
            clip.bottom = clip.top + m_clipRects[m_currentClipIndex].height();
            Device->SetScissorRect(&clip);
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Mismatched calls to Push/PopClipRect");
    }
}
