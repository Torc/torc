/* Class UIOpenGLBufferObjects
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
#include "uiopengltextures.h"
#include "uiopenglbufferobjects.h"

UIOpenGLBufferObjects::UIOpenGLBufferObjects()
  : m_usingVBOs(false),
    m_usingPBOs(false),
    m_mapBuffers(false),
    m_glMapBuffer(NULL),
    m_glBindBuffer(NULL),
    m_glGenBuffers(NULL),
    m_glBufferData(NULL),
    m_glUnmapBuffer(NULL),
    m_glDeleteBuffers(NULL)
{
}

UIOpenGLBufferObjects::~UIOpenGLBufferObjects()
{
}

bool UIOpenGLBufferObjects::InitialiseBufferObjects(const QString &Extensions, GLType Type)
{
    // buffer mapping
    m_glMapBuffer = (TORC_GLMAPBUFFERPROC)
        UIOpenGLWindow::GetProcAddress("glMapBuffer");
    m_glUnmapBuffer = (TORC_GLUNMAPBUFFERPROC)
        UIOpenGLWindow::GetProcAddress("glUnmapBuffer");

    bool mapping = m_glMapBuffer && m_glUnmapBuffer;

    // buffer objects
    m_glBindBuffer = (TORC_GLBINDBUFFERPROC)
        UIOpenGLWindow::GetProcAddress("glBindBuffer");
    m_glGenBuffers = (TORC_GLGENBUFFERSPROC)
        UIOpenGLWindow::GetProcAddress("glGenBuffers");
    m_glBufferData = (TORC_GLBUFFERDATAPROC)
        UIOpenGLWindow::GetProcAddress("glBufferData");
    m_glDeleteBuffers = (TORC_GLDELETEBUFFERSPROC)
        UIOpenGLWindow::GetProcAddress("glDeleteBuffers");

    bool buffers = m_glBindBuffer && m_glGenBuffers &&
                   m_glDeleteBuffers && m_glBufferData;

    // buffer mapping is an extension in ES 2.0
    if (Type == kGLOpenGL2ES && mapping && Extensions.contains("GL_OES_mapbuffer"))
    {
        m_mapBuffers = true;
    }

    // buffer mapping available from GL 1.5 and not available in ES 2.0
    if (Type == kGLOpenGL2 && mapping && buffers)
    {
        if (Extensions.contains("GL_ARB_pixel_buffer_object"))
            m_usingPBOs = true;
        m_mapBuffers = true;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Pixel buffer objects %1supported")
        .arg(m_usingPBOs ? "" : "NOT "));
    LOG(VB_GENERAL, LOG_INFO, QString("Buffer mapping %1supported")
        .arg(m_mapBuffers ? "" : "NOT "));

    if ((Extensions.contains("GL_ARB_vertex_buffer_object") && buffers) ||
        Type == kGLOpenGL2ES)
    {
        LOG(VB_GENERAL, LOG_INFO, "Vertex buffer objects supported");
        m_usingVBOs = true;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Vertex buffer objects NOT supported.");
    }

    return m_usingVBOs;
}

uint UIOpenGLBufferObjects::CreateVBO(void)
{
    if (!m_usingVBOs)
        return 0;

    GLuint newvbo;
    m_glGenBuffers(1, &newvbo);
    return newvbo;
}

bool UIOpenGLBufferObjects::CreateVBO(GLTexture *Texture)
{
    if (!m_usingVBOs || !Texture)
        return false;

    Texture->m_vbo = CreateVBO();
    return true;
}

bool UIOpenGLBufferObjects::CreatePBO(GLTexture *Texture)
{
    if (!m_usingPBOs || !Texture)
        return false;

    GLuint newpbo;
    m_glGenBuffers(1, &newpbo);
    Texture->m_pbo = newpbo;

    return true;
}
