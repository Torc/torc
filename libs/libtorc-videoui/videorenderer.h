#ifndef VIDEORENDERER_H
#define VIDEORENDERER_H

// Qt
#include <QRectF>

// Torc
#include "videoframe.h"

class UIWindow;
class UIDisplay;

class VideoRenderer
{
  public:
    static VideoRenderer*  Create(void);

  public:
    explicit VideoRenderer(UIWindow *Window);
    virtual ~VideoRenderer();

    virtual void           RenderFrame          (VideoFrame* Frame) = 0;
    virtual PixelFormat    PreferredPixelFormat (void) = 0;

  protected:
    bool                   UpdatePosition       (VideoFrame* Frame);

  protected:
    UIDisplay              *m_display;
    QRectF                  m_presentationRect;
};

class RenderFactory
{
  public:
    RenderFactory();
    virtual ~RenderFactory();
    static RenderFactory*  GetRenderFactory     (void);
    RenderFactory*         NextFactory          (void) const;
    virtual VideoRenderer* Create               (void) = 0;

  protected:
    static RenderFactory* gRenderFactory;
    RenderFactory*        nextRenderFactory;
};


#endif // VIDEORENDERER_H
