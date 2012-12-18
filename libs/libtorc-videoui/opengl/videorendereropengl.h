#ifndef VIDEORENDEREROPENGL_H
#define VIDEORENDEREROPENGL_H

// Torc
#include "videorenderer.h"

extern "C" {
#include "libavutil/pixfmt.h"
#include "libswscale/swscale.h"
}

class VideoColourSpace;
class UIOpenGLWindow;
class GLTexture;

class VideoRendererOpenGL : public VideoRenderer
{
  public:
    VideoRendererOpenGL(VideoColourSpace *ColourSpace, UIOpenGLWindow *Window);
    virtual ~VideoRendererOpenGL();

    void               RefreshFrame         (VideoFrame *Frame);
    void               RenderFrame          (void);
    PixelFormat        PreferredPixelFormat (void);
    void               CustomiseShader      (QByteArray &Source);

  protected:
    void               ResetOutput          (void);

  protected:
    UIOpenGLWindow    *m_openglWindow;
    PixelFormat        m_outputFormat;
    bool               m_validVideoFrame;
    bool               m_updateFrameVertices;
    GLTexture         *m_rawVideoTexture;
    GLTexture         *m_rgbVideoTexture;
    uint               m_rgbVideoBuffer;
    uint               m_videoShader;
    SwsContext        *m_conversionContext;
};

#endif // VIDEORENDEREROPENGL_H
