#ifndef VIDEORENDERER_H
#define VIDEORENDERER_H

// Qt
#include <QRectF>

// Torc
#include "videoframe.h"

class UIWindow;
class UIDisplay;
class VideoColourSpace;

class VideoRenderer
{
  public:
    static VideoRenderer*  Create(VideoColourSpace *ColourSpace);

  public:
    explicit VideoRenderer (VideoColourSpace *ColourSpace, UIWindow *Window);
    virtual ~VideoRenderer ();

    virtual void           RefreshFrame         (VideoFrame* Frame) = 0;
    virtual void           RenderFrame          (void) = 0;
    virtual PixelFormat    PreferredPixelFormat (void) = 0;
    void                   PlaybackFinished     (void);

  protected:
    virtual void           ResetOutput          (void) = 0;
    void                   UpdateRefreshRate    (VideoFrame* Frame);
    bool                   UpdatePosition       (VideoFrame* Frame);

  protected:
    UIWindow               *m_window;
    UIDisplay              *m_display;
    QRectF                  m_presentationRect;
    VideoColourSpace       *m_colourSpace;
};

class RenderFactory
{
  public:
    RenderFactory();
    virtual ~RenderFactory();
    static RenderFactory*  GetRenderFactory     (void);
    RenderFactory*         NextFactory          (void) const;
    virtual VideoRenderer* Create               (VideoColourSpace *ColourSpace) = 0;

  protected:
    static RenderFactory* gRenderFactory;
    RenderFactory*        nextRenderFactory;
};


#endif // VIDEORENDERER_H
