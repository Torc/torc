/* Class UIOpenGLView
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012
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
#include "uiopenglview.h"

UIOpenGLView::UIOpenGLView()
    : m_currentTransformIndex(0)
{
    m_currentProjection = &m_parallel;
    m_transforms[0] = UIOpenGLMatrix();
}

UIOpenGLView::~UIOpenGLView()
{
}

bool UIOpenGLView::InitialiseView(const QRect &Rect)
{
    SetViewPort(Rect);
    return true;
}

void UIOpenGLView::SetViewPort(const QRect &Rect, Projection Type /*= Parallel*/)
{
    if (Rect == m_viewport)
        return;

    m_viewport = Rect;
    glViewport(m_viewport.left(), m_viewport.top(),
               m_viewport.width(), m_viewport.height());

    if (Type != ViewPortOnly)
        SetProjection(Type);
}

QRect UIOpenGLView::GetViewPort(void)
{
    return m_viewport;
}

void UIOpenGLView::SetProjection(Projection Type)
{
    GLfloat left   = m_viewport.left();
    GLfloat top    = m_viewport.top();
    GLfloat right  = left + m_viewport.width();
    GLfloat bottom = top  + m_viewport.height();

    if (right <= 0 || bottom <= 0)
        return;

    // parallel/flat projection
    memset(&m_parallel.m[0][0], 0, sizeof(GLfloat) * 4 * 4);
    m_parallel.m[0][0] = 2.0 / (right - left);
    m_parallel.m[1][1] = 2.0 / (top - bottom);
    m_parallel.m[2][2] = 1.0;
    m_parallel.m[3][0] = -((right + left) / (right - left));
    m_parallel.m[3][1] = -((top + bottom) / (top - bottom));
    m_parallel.m[3][3] = 1.0;

    // perspective/3D projection
    qreal xadj = m_viewport.width()  / 2.0;
    qreal yadj = m_viewport.height() / 2.0;
    left   -= xadj; right  -= xadj;
    top    -= yadj; bottom -= yadj;

    GLfloat nearz = 1.0;
    GLfloat farz  = 2.0;

    UIOpenGLMatrix perspective;
    perspective.m[0][0] = (2.0 * nearz) / (right - left);
    perspective.m[1][1] = (2.0 * nearz) / (top - bottom);
    perspective.m[2][2] = - (farz + nearz) /  (farz - nearz);
    perspective.m[2][3] = -1.0;
    perspective.m[2][0] = (right + left) / (right - left);
    perspective.m[2][1] = (top + bottom) / (top - bottom);
    perspective.m[3][2] = - (2.0 * farz * nearz) / (farz - nearz);
    perspective.m[3][3] = 0.0;

    m_perspective.setToIdentity();
    m_perspective.translate(-xadj, -yadj);
    m_perspective *= perspective;

    // switch to correct stack
    m_currentProjection = Type == Perspective ? &m_perspective : &m_parallel;
}

bool UIOpenGLView::PushTransformation(const UIEffect *Effect,
                                      const QRectF   *Dest)
{
    if (!Effect || !Dest)
        return false;

    if (m_currentTransformIndex >= MAX_STACK_DEPTH)
    {
        LOG(VB_GENERAL, LOG_INFO, "Max stack depth exceeded");
        return false;
    }

    UIOpenGLMatrix matrix;
    matrix.alpha = m_transforms[m_currentTransformIndex].alpha * Effect->m_alpha;
    matrix.color = m_transforms[m_currentTransformIndex].color * Effect->m_color;

    if (Effect->m_hReflecting)
    {
        matrix.translate(0.0, -Effect->m_hReflection);
        matrix.reflect(false /*horiz*/);
        matrix.translate(0.0, Effect->m_hReflection);
    }
    else if (Effect->m_vReflecting)
    {
        matrix.translate(-Effect->m_vReflection, 0.0);
        matrix.reflect(true /*vert*/);
        matrix.translate(Effect->m_vReflection, 0.0);
    }

    if (Effect->m_hZoom != 1.0 || Effect->m_vZoom != 1.0 || Effect->m_rotation != 0.0)
    {
        QPointF center = Effect->GetCentre(Dest);
        matrix.translate(-center.x(), -center.y());
        matrix.rotate(Effect->m_rotation);
        matrix.scale(Effect->m_hZoom, Effect->m_vZoom);
        matrix.translate(center.x(), center.y());
    }

    if (Dest->left() != 0.0 || Dest->top() != 0.0)
        matrix.translate(Dest->left(), Dest->top());

    if (!Effect->m_detached)
        matrix *= m_transforms[m_currentTransformIndex];
    m_transforms[++m_currentTransformIndex] = matrix;

    return true;
}

void UIOpenGLView::PopTransformation(void)
{
    m_currentTransformIndex--;
}

void UIOpenGLView::PushClipRect(const QRect &Rect)
{
    if (m_clips.isEmpty())
        glEnable(GL_SCISSOR_TEST);

    m_clips.push(Rect);

    glScissor(Rect.left(), m_viewport.height() - Rect.top() - Rect.height(),
              Rect.width(), Rect.height());
}

void UIOpenGLView::PopClipRect(void)
{
    if (m_clips.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Mismatched calls to PopClip");
        return;
    }

    m_clips.pop();

    if (m_clips.isEmpty())
    {
        glDisable(GL_SCISSOR_TEST);
        return;
    }

    QRect rect = m_clips.top();
    glScissor(rect.left(), m_viewport.height() - rect.top() - rect.height(),
              rect.width(), rect.height());
}
