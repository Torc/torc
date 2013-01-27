#ifndef VIDEORENDERDIRECT3D9_H
#define VIDEORENDERDIRECT3D9_H

// Torc
#include "videorenderer.h"

extern "C" {
#include "libavutil/pixfmt.h"
#include "libswscale/swscale.h"
}

class VideoColourSpace;
class UIDirect3D9Window;
class D3D9Texture;
class D3D9Shader;

class VideoRenderDirect3D9 : public VideoRenderer
{
  public:
    VideoRenderDirect3D9(VideoColourSpace *ColourSpace, UIDirect3D9Window *Window);
    virtual ~VideoRenderDirect3D9();

    void               RefreshFrame         (VideoFrame *Frame, const QSizeF &Size);
    void               RenderFrame          (void);

  protected:
    void               ResetOutput          (void);

  protected:
    UIDirect3D9Window *m_direct3D9Window;
    D3D9Texture       *m_rawVideoTexture;
    D3D9Texture       *m_rgbVideoTexture;
    D3D9Shader        *m_yuvShader;
};

#endif // VIDEORENDERDIRECT3D9_H
