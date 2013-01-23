#ifndef VIDEORENDERER_H
#define VIDEORENDERER_H

// Qt
#include <QRectF>

// Torc
#include "videoframe.h"

class UIWindow;
class UIDisplay;
class VideoPlayer;
class VideoColourSpace;

class VideoRenderer
{
  public:
    static VideoRenderer*  Create(VideoColourSpace *ColourSpace);

  public:
    VideoRenderer          (VideoColourSpace *ColourSpace, UIWindow *Window);
    virtual ~VideoRenderer ();

    virtual void           RefreshFrame         (VideoFrame* Frame, const QSizeF &Size) = 0;
    virtual void           RenderFrame          (void) = 0;
    virtual PixelFormat    PreferredPixelFormat (void) = 0;
    void                   PlaybackFinished     (void);

    bool                   GetHighQualityScaling     (void);
    bool                   SetHighQualityScaling     (bool Enable);
    bool                   HighQualityScalingAllowed (void);
    bool                   HighQualityScalingEnabled (void);

  protected:
    virtual void           ResetOutput          (void);
    void                   UpdateRefreshRate    (VideoFrame* Frame);
    bool                   UpdatePosition       (VideoFrame* Frame, const QSizeF &Size);

  protected:
    UIWindow               *m_window;
    UIDisplay              *m_display;
    QRectF                  m_presentationRect;
    VideoColourSpace       *m_colourSpace;
    bool                    m_wantHighQualityScaling;
    bool                    m_allowHighQualityScaling;
    bool                    m_usingHighQualityScaling;
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
