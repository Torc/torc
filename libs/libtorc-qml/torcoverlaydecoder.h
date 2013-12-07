#ifndef TORCOVERLAYDECODER_H
#define TORCOVERLAYDECODER_H

// Qt
#include <QList>
#include <QImage>
#include <QSGNode>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>

// Torc
#include "torcconfig.h"
#include "videodecoder.h"
#include "torcqmlexport.h"
#include "torcvideooverlay.h"

#if defined(CONFIG_LIBASS) && CONFIG_LIBASS
extern "C"
{
#include <ass/ass.h>
}
#endif

class TORC_QML_PUBLIC TorcOverlayDecoder
{
  public:
    TorcOverlayDecoder();
    virtual ~TorcOverlayDecoder();

    void                     SetOverlayWindow     (QQuickWindow *Window);
    void                     ResetOverlays        (void);
    void                     GetOverlayImages     (VideoDecoder *Decoder, TorcVideoOverlayItem *Overlay, const QRectF &VideoBounds,
                                                   const QSizeF &VideoSize, double VideoFrameRate, qint64 VideoPts, QSGNode* Root);

  protected:
    void                     RenderImage          (QImage &Image, QRectF &Rect, QSGNode *Parent);
    void                     DecodeAVSubtitle     (VideoDecoder *Decoder, TorcVideoOverlayItem *Overlay, const QRectF &VideoBounds,
                                                   const QSizeF &VideoSize, double VideoFrameRate, qint64 VideoPts);
    void                     DecodeASSSubtitle    (const QRectF &VideoBounds, qint64 VideoPts);
    void                     DecodeBDOverlay      (VideoDecoder *Decoder, TorcVideoOverlayItem *Overlay, const QRectF &VideoBounds,
                                                   const QSizeF &VideoSize, double VideoFrameRate, qint64 VideoPts);
    void                     DecodeBDARGBOverlay  (VideoDecoder *Decoder, TorcVideoOverlayItem *Overlay, const QRectF &VideoBounds,
                                                   const QSizeF &VideoSize, double VideoFrameRate, qint64 VideoPts);

  protected:
    QQuickWindow            *m_window;
    QSGNode                 *m_lastRootNode;
    QSGNode                 *m_subtitleNode;
    QSGSimpleTextureNode    *m_graphicsNode;
    QSGSimpleTextureNode    *m_menusNode;
    QRectF                   m_bdOverlayRects[2];

#if defined(CONFIG_LIBASS) && CONFIG_LIBASS
  protected:
    bool                     InitialiseASSLibrary (void);
    void                     LoadASSFonts         (VideoDecoder *Decoder);
    void                     CleanupASSLibrary    (void);
    void                     InitialiseASSTrack   (int Track, VideoDecoder *Decoder, QSizeF &Bounds);
    void                     CleanupASSTrack      (void);

  protected:
    ASS_Library             *m_libassLibrary;
    ASS_Renderer            *m_libassRenderer;
    int                      m_libassTrackNumber;
    ASS_Track               *m_libassTrack;
    uint                     m_libassFontCount;
    QSizeF                   m_libassFrameSize;
#endif // CONFIG_LIBASS
};

#endif // TORCOVERLAYDECODER_H
