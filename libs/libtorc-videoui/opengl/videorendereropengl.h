#ifndef VIDEORENDEREROPENGL_H
#define VIDEORENDEREROPENGL_H

// Torc
#include "videocolourspace.h"
#include "videorenderer.h"

extern "C" {
#include "libavutil/pixfmt.h"
#include "libswscale/swscale.h"
}

class UIOpenGLWindow;
class GLTexture;

class VideoRendererOpenGL : public VideoRenderer
{
  public:
    VideoRendererOpenGL(UIOpenGLWindow *Window);
    virtual ~VideoRendererOpenGL();

    void               RenderFrame          (VideoFrame *Frame);
    PixelFormat        PreferredPixelFormat (void);
    void               CustomiseShader      (QByteArray &Source);

  protected:
    void               ResetOutput          (void);

  protected:
    UIOpenGLWindow    *m_openglWindow;
    PixelFormat        m_outputFormat;
    bool               m_validVideoFrame;
    GLTexture         *m_rawVideoTexture;
    GLTexture         *m_rgbVideoTexture;
    uint               m_rgbVideoBuffer;
    uint               m_videoShader;
    VideoColourSpace  *m_colourSpace;
    SwsContext        *m_conversionContext;
};

#endif // VIDEORENDEREROPENGL_H
