#ifndef UIOPENGLTEXTURES_H
#define UIOPENGLTEXTURES_H

// Qt
#include <QHash>

// Torc
#include "torcbaseuiexport.h"
#include "uiopengldefs.h"
#include "uiopenglbufferobjects.h"

#define TEX_OFFSET 12
#define TORC_UYVY  0x8A1F

class TORC_BASEUI_PUBLIC GLTexture
{
  public:
    GLTexture(GLuint Value);
    GLTexture();
   ~GLTexture();

    GLuint  m_val;
    GLuint  m_type;
    quint64 m_dataSize;
    quint64 m_internalDataSize;
    GLuint  m_dataType;
    GLuint  m_dataFmt;
    GLuint  m_internalFmt;
    GLuint  m_pbo;
    GLuint  m_vbo;
    bool    m_vboUpdated;
    GLuint  m_filter;
    GLuint  m_wrap;
    QSize   m_size;
    QSize   m_actualSize;
    bool    m_fullVertices;
    GLfloat m_vertexData[20];
};

class UIOpenGLTextures : public UIOpenGLBufferObjects
{
  public:
    UIOpenGLTextures();
    virtual ~UIOpenGLTextures();

    void* GetTextureBuffer      (GLTexture *Texture);
    void  UpdateTexture         (GLTexture *Texture, const void *Buffer);
    GLTexture* CreateTexture    (QSize ActualSize,
                                 bool  UsePBO,
                                 uint  Type,
                                 uint  DataType = GL_UNSIGNED_BYTE,
                                 uint  DataFmt  = GL_BGRA,
                                 uint  InternalFmt = GL_RGBA8,
                                 uint  Filter = GL_LINEAR,
                                 uint  Wrap = GL_CLAMP_TO_EDGE);
    void  SetTextureFilters     (GLTexture *Texture, uint Filt, uint Wrap);
    void  DeleteTexture         (uint Texture);
    int   GetMaxTextureSize     (void);
    bool  IsRectTexture         (uint Type);
    QSize GetTextureSize        (uint Type, const QSize &Size);
    void  ActiveTexture         (int  ActiveTexture);
    void  UpdateTextureVertices (GLTexture *Texture, const QSizeF *Source, const QRectF *Dest);
    bool  ClearTexture          (GLTexture *Texture);
    uint  GetBufferSize         (QSize Size, uint Format, uint Type);

  protected:
    bool  InitialiseTextures    (const QString &Extensions, GLType Type);
    void  DeleteTextures        (void);

  protected:
    bool                        m_allowRects;
    bool                        m_mipMapping;
    int                         m_activeTexture;
    int                         m_activeTextureType;
    int                         m_maxTextureSize;
    int                         m_defaultTextureType;
    QHash<GLuint,GLTexture*>    m_textures;

    TORC_GLACTIVETEXTUREPROC    m_glActiveTexture;
};

#endif // UIOPENGLTEXTURES_H
