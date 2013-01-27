#ifndef VIDEORENDEREROPENGL_H
#define VIDEORENDEREROPENGL_H

// Torc
#include "videorenderer.h"

class VideoColourSpace;
class UIOpenGLWindow;
class GLTexture;

class VideoRendererOpenGL : public VideoRenderer
{
  public:
    VideoRendererOpenGL(VideoColourSpace *ColourSpace, UIOpenGLWindow *Window);
    virtual ~VideoRendererOpenGL();

    void               RefreshFrame         (VideoFrame *Frame, const QSizeF &Size);
    void               RenderFrame          (void);
    void               CustomiseShader      (QByteArray &Source);

  protected:
    void               ResetOutput          (void);

  protected:
    UIOpenGLWindow    *m_openglWindow;
    GLTexture         *m_rawVideoTexture;
    GLTexture         *m_rgbVideoTexture;
    uint               m_rgbVideoBuffer;
    uint               m_yuvShader;
    uint               m_rgbShader;
    uint               m_bicubicShader;
    SwsContext        *m_conversionContext;
};

#endif // VIDEORENDEREROPENGL_H
