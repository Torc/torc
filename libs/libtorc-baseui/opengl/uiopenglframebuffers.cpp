/* Class UIOpenGLFrameBuffers
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
#include "uiopenglwindow.h"
#include "uiopengltextures.h"
#include "uiopenglframebuffers.h"

UIOpenGLFramebuffers::UIOpenGLFramebuffers()
  : UIOpenGLView(), UIOpenGLTextures(),
    m_usingFramebuffers(false),
    m_active_fb(0),
    m_glGenFramebuffers(NULL),
    m_glBindFramebuffer(NULL),
    m_glFramebufferTexture2D(NULL),
    m_glCheckFramebufferStatus(NULL),
    m_glDeleteFramebuffers(NULL)
{
}

UIOpenGLFramebuffers::~UIOpenGLFramebuffers()
{
    DeleteFrameBuffers();
}

bool UIOpenGLFramebuffers::InitialiseFramebuffers(const QString &Extensions, GLType Type)
{
    m_glGenFramebuffers = (TORC_GLGENFRAMEBUFFERSPROC)
        UIOpenGLWindow::GetProcAddress("glGenFramebuffers");
    m_glBindFramebuffer = (TORC_GLBINDFRAMEBUFFERPROC)
        UIOpenGLWindow::GetProcAddress("glBindFramebuffer");
    m_glFramebufferTexture2D = (TORC_GLFRAMEBUFFERTEXTURE2DPROC)
        UIOpenGLWindow::GetProcAddress("glFramebufferTexture2D");
    m_glCheckFramebufferStatus = (TORC_GLCHECKFRAMEBUFFERSTATUSPROC)
        UIOpenGLWindow::GetProcAddress("glCheckFramebufferStatus");
    m_glDeleteFramebuffers = (TORC_GLDELETEFRAMEBUFFERSPROC)
        UIOpenGLWindow::GetProcAddress("glDeleteFramebuffers");

    bool buffers = m_glGenFramebuffers && m_glBindFramebuffer &&
                   m_glFramebufferTexture2D &&
                   m_glCheckFramebufferStatus &&
                   m_glDeleteFramebuffers;

    m_usingFramebuffers = buffers &&
                          (Extensions.contains("GL_ARB_framebuffer_object") ||
                           Extensions.contains("GL_EXT_framebuffer_object") ||
                           kGLOpenGL2ES == Type);

    LOG(VB_GENERAL, LOG_INFO, QString("Framebuffer objects %1supported")
        .arg(m_usingFramebuffers ? "" : "NOT "));

    return m_usingFramebuffers;
}

void UIOpenGLFramebuffers::BindFramebuffer(uint FrameBuffer)
{
    if (FrameBuffer == (uint)m_active_fb)
        return;

    if (FrameBuffer && !m_framebuffers.contains(FrameBuffer))
        return;

    m_glBindFramebuffer(GL_FRAMEBUFFER, FrameBuffer);
    m_active_fb = FrameBuffer;
}

void UIOpenGLFramebuffers::ClearFramebuffer(void)
{
    glClear(GL_COLOR_BUFFER_BIT);
}

bool UIOpenGLFramebuffers::CreateFrameBuffer(uint &FrameBuffer, uint Texture)
{
    if (!m_usingFramebuffers || !Texture)
        return false;

    QSize size = m_textures[Texture]->m_size;
    GLuint glfb;

    QRect tmp_viewport = m_viewport;
    glViewport(0, 0, size.width(), size.height());
    m_glGenFramebuffers(1, &glfb);
    m_glBindFramebuffer(GL_FRAMEBUFFER, glfb);
    glBindTexture(m_textures[Texture]->m_type, Texture);
    glTexImage2D(m_textures[Texture]->m_type, 0, m_textures[Texture]->m_internalFmt,
                 (GLint)size.width(), (GLint)size.height(), 0,
                 m_textures[Texture]->m_dataFmt, m_textures[Texture]->m_dataType, NULL);
    m_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             m_textures[Texture]->m_type, Texture, 0);

    GLenum status;
    status = m_glCheckFramebufferStatus(GL_FRAMEBUFFER);
    m_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(tmp_viewport.left(), tmp_viewport.top(),
               tmp_viewport.width(), tmp_viewport.height());

    bool success = false;
    switch (status)
    {
        case GL_FRAMEBUFFER_COMPLETE:
            LOG(VB_PLAYBACK, LOG_INFO, QString("Created frame buffer object (%1x%2)")
                    .arg(size.width()).arg(size.height()));
            success = true;
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            LOG(VB_PLAYBACK, LOG_INFO, "Frame buffer incomplete_ATTACHMENT");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            LOG(VB_PLAYBACK, LOG_INFO, "Frame buffer incomplete_MISSING_ATTACHMENT");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DUPLICATE_ATTACHMENT:
            LOG(VB_PLAYBACK, LOG_INFO, "Frame buffer incomplete_DUPLICATE_ATTACHMENT");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
            LOG(VB_PLAYBACK, LOG_INFO, "Frame buffer incomplete_DIMENSIONS");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_FORMATS:
            LOG(VB_PLAYBACK, LOG_INFO, "Frame buffer incomplete_FORMATS");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            LOG(VB_PLAYBACK, LOG_INFO, "Frame buffer incomplete_DRAW_BUFFER");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            LOG(VB_PLAYBACK, LOG_INFO, "Frame buffer incomplete_READ_BUFFER");
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED:
            LOG(VB_PLAYBACK, LOG_INFO, "Frame buffer unsupported");
            break;
        default:
            LOG(VB_PLAYBACK, LOG_INFO, QString("Unknown frame buffer error %1").arg(status));
    }

    if (success)
        m_framebuffers.push_back(glfb);
    else
        m_glDeleteFramebuffers(1, &glfb);

    FrameBuffer = glfb;
    return success;
}

void UIOpenGLFramebuffers::DeleteFrameBuffer(uint FrameBuffer)
{
    if (!m_framebuffers.contains(FrameBuffer))
        return;

    QVector<GLuint>::iterator it;
    for (it = m_framebuffers.begin(); it != m_framebuffers.end(); ++it)
    {
        if (*it == FrameBuffer)
        {
            m_glDeleteFramebuffers(1, &(*it));
            m_framebuffers.erase(it);
            break;
        }
    }
}


void UIOpenGLFramebuffers::DeleteFrameBuffers(void)
{
    QVector<GLuint>::iterator it;
    for (it = m_framebuffers.begin(); it != m_framebuffers.end(); ++it)
        m_glDeleteFramebuffers(1, &(*(it)));
    m_framebuffers.clear();
}
