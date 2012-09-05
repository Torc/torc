#ifndef UIOPENGLFENCE_H
#define UIOPENGLFENCE_H

// Torc
#include "uiopengldefs.h"

class UIOpenGLFence
{
  public:
    UIOpenGLFence();
    virtual ~UIOpenGLFence();

  protected:
    bool InitialiseFence         (const QString &Extensions);
    void Flush                   (bool UseFence);
    bool CreateFence             (void);
    void DeleteFence             (void);

  protected:
    bool                         m_apple;
    bool                         m_nvidia;
    GLuint                       m_fence;

    // NV_fence
    TORC_GLGENFENCESNVPROC       m_glGenFencesNV;
    TORC_GLDELETEFENCESNVPROC    m_glDeleteFencesNV;
    TORC_GLSETFENCENVPROC        m_glSetFenceNV;
    TORC_GLFINISHFENCENVPROC     m_glFinishFenceNV;

    // APPLE_fence
    TORC_GLGENFENCESAPPLEPROC    m_glGenFencesAPPLE;
    TORC_GLDELETEFENCESAPPLEPROC m_glDeleteFencesAPPLE;
    TORC_GLSETFENCEAPPLEPROC     m_glSetFenceAPPLE;
    TORC_GLFINISHFENCEAPPLEPROC  m_glFinishFenceAPPLE;
};

#endif // UIOPENGLFENCE_H
