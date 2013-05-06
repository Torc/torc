/* Class UIOpenGLTextures
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

GLTexture::GLTexture()
  : m_val(0),
    m_type(GL_TEXTURE_2D),
    m_dataSize(0),
    m_internalDataSize(0),
    m_dataType(GL_UNSIGNED_BYTE),
    m_dataFmt(GL_BGRA),
    m_internalFmt(GL_RGBA8),
    m_pbo(0),
    m_vbo(0),
    m_vboUpdated(false),
    m_filter(GL_LINEAR),
    m_wrap(GL_CLAMP_TO_EDGE),
    m_size(0,0),
    m_actualSize(0,0),
    m_fullVertices(false)
{
    memset(&m_vertexData, 0, sizeof(GLfloat) * 16);
}

GLTexture::GLTexture(GLuint Value)
  : m_val(Value),
    m_type(GL_TEXTURE_2D),
    m_dataSize(0),
    m_internalDataSize(0),
    m_dataType(GL_UNSIGNED_BYTE),
    m_dataFmt(GL_BGRA),
    m_internalFmt(GL_RGBA8),
    m_pbo(0),
    m_vbo(0),
    m_vboUpdated(false),
    m_filter(GL_LINEAR),
    m_wrap(GL_CLAMP_TO_EDGE),
    m_size(0,0),
    m_actualSize(0,0),
    m_fullVertices(false)
{
    memset(&m_vertexData, 0, sizeof(GLfloat) * 16);
}

GLTexture::~GLTexture()
{
}

UIOpenGLTextures::UIOpenGLTextures()
  : UIOpenGLBufferObjects(),
    m_allowRects(false),
    m_mipMapping(false),
    m_activeTexture(0),
    m_activeTextureType(0),
    m_maxTextureSize(0),
    m_defaultTextureType(GL_TEXTURE_2D),
    m_rectTextureType(GL_TEXTURE_2D),
    m_glActiveTexture(NULL)
{
}

UIOpenGLTextures::~UIOpenGLTextures()
{
    DeleteTextures();
}

bool UIOpenGLTextures::InitialiseTextures(const QString &Extensions, GLType Type)
{
    InitialiseBufferObjects(Extensions, Type);

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_maxTextureSize);
    m_maxTextureSize = (m_maxTextureSize) ? m_maxTextureSize : 512;

    static bool rects = true;
    static bool check = true;
    if (check)
    {
        check = false;
        rects = qgetenv("OPENGL_NORECT").isEmpty();
        if (!rects)
            LOG(VB_GENERAL, LOG_INFO, "Disabling NPOT textures.");
    }

    if (Extensions.contains("GL_NV_texture_rectangle") && rects)
        m_rectTextureType = GL_TEXTURE_RECTANGLE_NV;
    else if (Extensions.contains("GL_ARB_texture_rectangle") && rects)
        m_rectTextureType = GL_TEXTURE_RECTANGLE_ARB;
    else if (Extensions.contains("GL_EXT_texture_rectangle") && rects)
        m_rectTextureType = GL_TEXTURE_RECTANGLE_EXT;

    if (Extensions.contains("GL_ARB_texture_non_power_of_two") && rects)
        m_allowRects = true;
    else if (Extensions.contains("GL_OES_texture_npot") && Type == kGLOpenGL2ES && rects)
        m_allowRects = true;
    else if (IsRectTexture(m_rectTextureType))
        m_defaultTextureType = m_rectTextureType;

    m_mipMapping = Extensions.contains("GL_SGIS_generate_mipmap");

    if (m_allowRects)
    {
        LOG(VB_GENERAL, LOG_INFO, "Using 'non power of two' textures");
        if (IsRectTexture(m_rectTextureType))
            LOG(VB_GENERAL, LOG_INFO, "Rectangular textures available");
    }
    else if (m_defaultTextureType != GL_TEXTURE_2D)
    {
        LOG(VB_GENERAL, LOG_INFO, "Using rectangular textures");
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, "Using default texture type");
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Max texture size: %1 x %2")
            .arg(m_maxTextureSize).arg(m_maxTextureSize));
    LOG(VB_GENERAL, LOG_INFO, QString("Mipmapping %1available")
            .arg(m_mipMapping ? "" : "not "));

    return true;
}

int UIOpenGLTextures::GetMaxTextureSize(void)
{
    return m_maxTextureSize;
}

QSize UIOpenGLTextures::GetTextureSize(uint Type, const QSize &Size)
{
    if (IsRectTexture(Type) || m_allowRects)
        return Size;

    int w = 64;
    int h = 64;

    while (w < Size.width())
        w *= 2;

    while (h < Size.height())
        h *= 2;

    return QSize(w, h);
}

bool UIOpenGLTextures::IsRectTexture(uint Type)
{
    if (Type == GL_TEXTURE_RECTANGLE_NV || Type == GL_TEXTURE_RECTANGLE_ARB ||
        Type == GL_TEXTURE_RECTANGLE_EXT)
    {
        return true;
    }

    return false;
}

void UIOpenGLTextures::ActiveTexture(int ActiveTexture)
{
    if (m_activeTexture != ActiveTexture)
    {
        m_glActiveTexture(ActiveTexture);
        m_activeTexture = ActiveTexture;
    }
}

void UIOpenGLTextures::UpdateTextureVertices(GLTexture    *Texture,
                                             const QSizeF *Source,
                                             const QRectF *Dest)
{
    if (!Texture)
        return;

    GLfloat *data = Texture->m_vertexData;

    data[0 + TEX_OFFSET] = 0;
    data[1 + TEX_OFFSET] = Source->height();

    data[6 + TEX_OFFSET] = Source->width();
    data[7 + TEX_OFFSET] = 0;

    if (!IsRectTexture(Texture->m_type))
    {
        data[0 + TEX_OFFSET] /= (GLfloat)Texture->m_size.width();
        data[6 + TEX_OFFSET] /= (GLfloat)Texture->m_size.width();
        data[1 + TEX_OFFSET] /= (GLfloat)Texture->m_size.height();
        data[7 + TEX_OFFSET] /= (GLfloat)Texture->m_size.height();
    }

    data[2 + TEX_OFFSET] = data[0 + TEX_OFFSET];
    data[3 + TEX_OFFSET] = data[7 + TEX_OFFSET];
    data[4 + TEX_OFFSET] = data[6 + TEX_OFFSET];
    data[5 + TEX_OFFSET] = data[1 + TEX_OFFSET];

    bool full = Texture->m_fullVertices;
    data[3]  = data[0]  = full ? Dest->left() : 0.0;
    data[7]  = data[1]  = full ? Dest->top()  : 0.0;
    data[11] = data[8]  = data[5] = data[2] = -1.0000000001;
    data[6]  = data[9]  = full ? Dest->left() + Dest->width() : Dest->width();
    data[4]  = data[10] = full ? Dest->top() + Dest->height() : Dest->height();

    Texture->m_vboUpdated = true;
}

bool UIOpenGLTextures::ClearTexture(GLTexture *Texture)
{
    if (!Texture)
        return false;

    QSize size = Texture->m_size;
    uint buffsize = GetBufferSize(size, Texture->m_dataFmt,
                                  Texture->m_dataType);

    if (!buffsize)
        return false;

    unsigned char *scratch = new unsigned char[buffsize];
    if (!scratch)
        return false;

    memset(scratch, 0, buffsize);
    glTexImage2D(Texture->m_type, 0, Texture->m_internalFmt,
                 size.width(), size.height(), 0, Texture->m_dataFmt,
                 Texture->m_dataType, scratch);
    delete [] scratch;

    return true;
}

uint UIOpenGLTextures::GetBufferSize(QSize Size, uint Format, uint Type)
{
    uint bytes;
    uint bpp;

    if (Format == GL_BGRA || Format == GL_RGBA || Format == GL_RGBA8)
    {
        bpp = 4;
    }
    else if (Format == GL_YCBCR_MESA || Format == GL_YCBCR_422_APPLE ||
             Format == TORC_UYVY)
    {
        bpp = 2;
    }
    else
    {
        LOG(VB_GENERAL, LOG_WARNING, "Unknown OpenGL data format");
        bpp = 0;
    }

    switch (Type)
    {
        case GL_UNSIGNED_BYTE:
            bytes = sizeof(GLubyte);
            break;
        case GL_UNSIGNED_SHORT_8_8_MESA:
            bytes = sizeof(GLushort);
            break;
        case GL_FLOAT:
            bytes = sizeof(GLfloat);
            break;
        default:
            LOG(VB_GENERAL, LOG_WARNING, "Unknown OpenGL data format");
            bytes = 0;
    }

    if (!bpp || !bytes || Size.width() < 1 || Size.height() < 1)
        return 0;

    return Size.width() * Size.height() * bpp * bytes;
}

int UIOpenGLTextures::GetRectTextureType(void)
{
    return m_rectTextureType;
}

void UIOpenGLTextures::DeleteTextures(void)
{
    QHash<GLuint, GLTexture*>::iterator it;
    for (it = m_textures.begin(); it !=m_textures.end(); ++it)
    {
        glDeleteTextures(1, &(it.key()));
        if (it.value()->m_pbo)
            m_glDeleteBuffers(1, &(it.value()->m_pbo));
        if (it.value()->m_vbo)
            m_glDeleteBuffers(1, &(it.value()->m_vbo));
        delete it.value();
    }
    m_textures.clear();
}

void* UIOpenGLTextures::GetTextureBuffer(GLTexture *Texture)
{
    if (!Texture)
        return NULL;

    glBindTexture(Texture->m_type, Texture->m_val);

    if (Texture->m_pbo)
    {
        m_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, Texture->m_pbo);
        m_glBufferData(GL_PIXEL_UNPACK_BUFFER, Texture->m_dataSize,
                       NULL, GL_STREAM_DRAW);
        return m_glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    }

    return NULL;
}

void UIOpenGLTextures::UpdateTexture(GLTexture *Texture, const void *Buffer)
{
    // N.B. GetTextureBuffer must be called first
    if (!Texture)
        return;

    QSize size = Texture->m_actualSize;

    if (Texture->m_pbo)
    {
        m_glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        glTexSubImage2D(Texture->m_type, 0, 0, 0, size.width(),
                        size.height(), Texture->m_dataFmt,
                        Texture->m_dataType, 0);
        m_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }
    else
    {
        glTexSubImage2D(Texture->m_type, 0, 0, 0, size.width(),
                        size.height(), Texture->m_dataFmt,
                        Texture->m_dataType, Buffer);
    }
}

GLTexture* UIOpenGLTextures::CreateTexture(QSize ActualSize, bool UsePBO,
                                           uint Type, uint DataType,
                                           uint DataFmt, uint InternalFmt,
                                           uint Filter, uint Wrap)
{
    if (!Type)
        Type = m_defaultTextureType;

    QSize totalsize = GetTextureSize(Type, ActualSize);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(Type, tex);

    if (tex)
    {
        GLTexture *texture = new GLTexture(tex);
        texture->m_type        = Type;
        texture->m_dataType    = DataType;
        texture->m_dataFmt     = DataFmt;
        texture->m_internalFmt = InternalFmt;
        texture->m_size        = totalsize;
        texture->m_actualSize  = ActualSize;
        texture->m_dataSize    = GetBufferSize(ActualSize, DataFmt, DataType);
        texture->m_internalDataSize = GetBufferSize(totalsize, InternalFmt, GL_UNSIGNED_BYTE);
        SetTextureFilters(texture, Filter, Wrap);
        ClearTexture(texture);
        if (UsePBO)
            CreatePBO(texture);
        CreateVBO(texture);

        m_textures.insert(tex, texture);

        return texture;
    }

    return NULL;
}

void UIOpenGLTextures::SetTextureFilters(GLTexture *Texture, uint Filt, uint Wrap)
{
    if (!Texture)
        return;

    bool mipmaps = m_mipMapping && !IsRectTexture(Texture->m_type);
    if (Filt == GL_LINEAR_MIPMAP_LINEAR && !mipmaps)
        Filt = GL_LINEAR;

    Texture->m_filter = Filt;
    Texture->m_wrap   = Wrap;
    uint type = Texture->m_type;
    glBindTexture(type, Texture->m_val);
    uint mag_filt = Filt;
    if (Filt == GL_LINEAR_MIPMAP_LINEAR)
    {
        mag_filt = GL_LINEAR;
        glHint(GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST);
        glTexParameteri(type, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
    }
    glTexParameteri(type, GL_TEXTURE_MIN_FILTER, Filt);
    glTexParameteri(type, GL_TEXTURE_MAG_FILTER, mag_filt);
    glTexParameteri(type, GL_TEXTURE_WRAP_S,     Wrap);
    glTexParameteri(type, GL_TEXTURE_WRAP_T,     Wrap);
}

void UIOpenGLTextures::DeleteTexture(uint Texture)
{
    if (!m_textures.contains(Texture))
        return;

    GLuint gltex = Texture;
    glDeleteTextures(1, &gltex);
    if (m_textures[Texture]->m_pbo)
        m_glDeleteBuffers(1, &(m_textures[Texture]->m_pbo));
    if (m_textures[Texture]->m_vbo)
        m_glDeleteBuffers(1, &(m_textures[Texture]->m_vbo));
    delete m_textures.value(Texture);
    m_textures.remove(Texture);
}
