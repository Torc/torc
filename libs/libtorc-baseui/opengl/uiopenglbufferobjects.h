#ifndef UIOPENGLBUFFEROBJECTS_H
#define UIOPENGLBUFFEROBJECTS_H

// Qt
#include <QMap>

// Torc
#include "uiopengldefs.h"

static const GLuint kVertexOffset  = 0;
static const GLuint kTextureOffset = 12 * sizeof(GLfloat);
static const GLuint kVertexSize    = 20 * sizeof(GLfloat);

class GLTexture;

class UIOpenGLBufferObjects
{
  public:
    UIOpenGLBufferObjects();
    virtual ~UIOpenGLBufferObjects();

  protected:
    bool InitialiseBufferObjects (const QString &Extensions, GLType Type);
    uint CreateVBO               (void);
    bool CreateVBO               (GLTexture *Texture);
    bool CreatePBO               (GLTexture *Texture);

  protected:
    bool                         m_usingVBOs;
    bool                         m_usingPBOs;
    bool                         m_mapBuffers;

    TORC_GLMAPBUFFERPROC         m_glMapBuffer;
    TORC_GLBINDBUFFERPROC        m_glBindBuffer;
    TORC_GLGENBUFFERSPROC        m_glGenBuffers;
    TORC_GLBUFFERDATAPROC        m_glBufferData;
    TORC_GLUNMAPBUFFERPROC       m_glUnmapBuffer;
    TORC_GLDELETEBUFFERSPROC     m_glDeleteBuffers;
};

#endif // UIOPENGLBUFFEROBJECTS_H
