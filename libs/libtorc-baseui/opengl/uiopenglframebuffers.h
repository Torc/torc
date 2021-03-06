#ifndef UIOPENGLFRAMEBUFFERS_H
#define UIOPENGLFRAMEBUFFERS_H

// Qt
#include <QVector>

// Torc
#include "torcbaseuiexport.h"
#include "uiopengldefs.h"
#include "uiopengltextures.h"
#include "uiopenglview.h"

class GLTexture;

class TORC_BASEUI_PUBLIC UIOpenGLFramebuffers : public UIOpenGLView, public UIOpenGLTextures
{
  public:
    UIOpenGLFramebuffers();
    virtual ~UIOpenGLFramebuffers();

    void BindFramebuffer              (uint FrameBuffer);
    void ClearFramebuffer             (void);
    bool CreateFrameBuffer            (uint &FrameBuffer, uint Texture);
    void DeleteFrameBuffer            (uint FrameBuffer);

  protected:
    bool InitialiseFramebuffers       (const QString &Extensions, GLType Type);
    void DeleteFrameBuffers           (void);

  protected:
    bool                              m_usingFramebuffers;
    uint                              m_activeFramebuffer;
    QVector<GLuint>                   m_framebuffers;

    TORC_GLGENFRAMEBUFFERSPROC        m_glGenFramebuffers;
    TORC_GLBINDFRAMEBUFFERPROC        m_glBindFramebuffer;
    TORC_GLFRAMEBUFFERTEXTURE2DPROC   m_glFramebufferTexture2D;
    TORC_GLCHECKFRAMEBUFFERSTATUSPROC m_glCheckFramebufferStatus;
    TORC_GLDELETEFRAMEBUFFERSPROC     m_glDeleteFramebuffers;
};

#endif // UIOPENGLFRAMEBUFFERS_H
