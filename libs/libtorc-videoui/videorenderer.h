#ifndef VIDEORENDERER_H
#define VIDEORENDERER_H

// Qt
#include <QRectF>

// Torc
#include "videoframe.h"

extern "C" {
#include "libavutil/pixfmt.h"
#include "libswscale/swscale.h"
}

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
    virtual bool           DisplayReset         (void);
    AVPixelFormat          PreferredPixelFormat (void);
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
    AVPixelFormat           m_outputFormat;
    AVPixelFormat           m_lastInputFormat;
    bool                    m_validVideoFrame;
    double                  m_lastFrameAspectRatio;
    int                     m_lastFrameWidth;
    int                     m_lastFrameHeight;
    QRectF                  m_presentationRect;
    VideoColourSpace       *m_colourSpace;
    bool                    m_updateFrameVertices;
    bool                    m_wantHighQualityScaling;
    bool                    m_allowHighQualityScaling;
    bool                    m_usingHighQualityScaling;
    SwsContext             *m_conversionContext;
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
