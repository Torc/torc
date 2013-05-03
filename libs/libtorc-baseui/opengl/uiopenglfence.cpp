/* Class UIOpenGLFence
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012-13
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
#include "uiopenglwindow.h"
#include "uiopenglfence.h"

UIOpenGLFence::UIOpenGLFence()
  : m_apple(false),
    m_nvidia(false),
    m_fence(0),
    m_glGenFencesNV(NULL),
    m_glDeleteFencesNV(NULL),
    m_glSetFenceNV(NULL),
    m_glFinishFenceNV(NULL),
    m_glGenFencesAPPLE(NULL),
    m_glDeleteFencesAPPLE(NULL),
    m_glSetFenceAPPLE(NULL),
    m_glFinishFenceAPPLE(NULL)
{
}

UIOpenGLFence::~UIOpenGLFence()
{
    DeleteFence();
}

bool UIOpenGLFence::InitialiseFence(const QString &Extensions)
{
    m_glGenFencesNV = (TORC_GLGENFENCESNVPROC)
        UIOpenGLWindow::GetProcAddress("glGenFencesNV");
    m_glDeleteFencesNV = (TORC_GLDELETEFENCESNVPROC)
        UIOpenGLWindow::GetProcAddress("glDeleteFencesNV");
    m_glSetFenceNV = (TORC_GLSETFENCENVPROC)
        UIOpenGLWindow::GetProcAddress("glSetFenceNV");
    m_glFinishFenceNV = (TORC_GLFINISHFENCENVPROC)
        UIOpenGLWindow::GetProcAddress("glFinishFenceNV");
    m_glGenFencesAPPLE = (TORC_GLGENFENCESAPPLEPROC)
        UIOpenGLWindow::GetProcAddress("glGenFencesAPPLE");
    m_glDeleteFencesAPPLE = (TORC_GLDELETEFENCESAPPLEPROC)
        UIOpenGLWindow::GetProcAddress("glDeleteFencesAPPLE");
    m_glSetFenceAPPLE = (TORC_GLSETFENCEAPPLEPROC)
        UIOpenGLWindow::GetProcAddress("glSetFenceAPPLE");
    m_glFinishFenceAPPLE = (TORC_GLFINISHFENCEAPPLEPROC)
        UIOpenGLWindow::GetProcAddress("glFinishFenceAPPLE");

    if (Extensions.contains("GL_NV_fence") &&
        m_glGenFencesNV && m_glDeleteFencesNV &&
        m_glSetFenceNV  && m_glFinishFenceNV)
    {
        m_nvidia = true;
        LOG(VB_GENERAL, LOG_INFO, "NV fence available.");
    }

    if (Extensions.contains("GL_APPLE_fence") &&
        m_glGenFencesAPPLE && m_glDeleteFencesAPPLE &&
        m_glSetFenceAPPLE  && m_glFinishFenceAPPLE)
    {
        m_apple = true;
        LOG(VB_GENERAL, LOG_INFO, "Apple fence available.");
    }

    return m_apple || m_nvidia;
}

void UIOpenGLFence::Flush(bool UseFence)
{
    if (UseFence && m_apple && m_fence)
    {
        m_glSetFenceAPPLE(m_fence);
        m_glFinishFenceAPPLE(m_fence);
    }
    else if (UseFence && m_nvidia && m_fence)
    {
        m_glSetFenceNV(m_fence, GL_ALL_COMPLETED_NV);
        m_glFinishFenceNV(m_fence);
    }
    else
    {
        glFlush();
    }
}

bool UIOpenGLFence::CreateFence(void)
{
    if (m_apple)
    {
        m_glGenFencesAPPLE(1, &m_fence);
        if (m_fence)
            LOG(VB_PLAYBACK, LOG_INFO, "Using GL_APPLE_fence");
    }
    else if (m_nvidia)
    {
        m_glGenFencesNV(1, &m_fence);
        if (m_fence)
            LOG(VB_PLAYBACK, LOG_INFO, "Using GL_NV_fence");
    }

    return m_fence;
}

void UIOpenGLFence::DeleteFence(void)
{
    if (m_fence && m_apple)
        m_glDeleteFencesAPPLE(1, &m_fence);
    else if (m_fence && m_nvidia)
        m_glDeleteFencesNV(1, &m_fence);
    m_fence = 0;
}
