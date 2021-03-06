#ifndef TORCVIDEOOVERLAY_H
#define TORCVIDEOOVERLAY_H

// Qt
#include <QMap>
#include <QRect>
#include <QMutex>
#include <QLocale>

// Torc
#include "libbluray/src/libbluray/decoders/overlay.h"
#include "torcvideoexport.h"

class TORC_VIDEO_PUBLIC TorcVideoOverlayItem
{
  public:
    typedef enum
    {
        Subtitle = 0x0000,
        Graphics = 0x1000,
        Menu     = 0x2000
    } OverlayType;

    typedef enum
    {
        SubRectScreen = 0,
        SubRectVideo
    } OverlaySubrectType;

    typedef enum
    {
        BDOverlay,
        RawUTF8,
        FFmpegSubtitle,
        BDARGBOverlay
    } OverlayBufferType;

  public:
    TorcVideoOverlayItem (void *Buffer, int Index, QLocale::Language Language, int Flags, bool FixPosition);
    TorcVideoOverlayItem (QList<bd_overlay_s*> *Overlays);
    TorcVideoOverlayItem (QList<bd_argb_overlay_s*> *Overlays);
   ~TorcVideoOverlayItem ();

    bool                 IsValid       (void);

  public:
    bool                 m_valid;
    int                  m_streamIndex;
    OverlayType          m_type;
    OverlayBufferType    m_bufferType;
    int                  m_flags;
    QLocale::Language    m_language;
    OverlaySubrectType   m_subRectType;
    QRect                m_displayRect;
    QRect                m_displaySubRect;
    bool                 m_fixRect;
    qint64               m_startPts;
    qint64               m_endPts;
    void                *m_buffer;

  private:
    TorcVideoOverlayItem ();
};

#define TORC_ALL_OVERLAYS (TorcVideoOverlayItem::Subtitle | TorcVideoOverlayItem::Graphics | TorcVideoOverlayItem::Menu)

class TORC_VIDEO_PUBLIC TorcVideoOverlay
{
  public:
    TorcVideoOverlay();
    virtual ~TorcVideoOverlay();

    void                                AddOverlay         (TorcVideoOverlayItem *Item);
    void                                ClearQueuedOverlays(int Types = TORC_ALL_OVERLAYS);
    QList<TorcVideoOverlayItem*>        GetNewOverlays     (qint64 VideoPts);

  protected:
    bool                                m_ignoreOverlays;
    QMap<qint64, TorcVideoOverlayItem*> m_timedOverlays;
    QList<TorcVideoOverlayItem*>        m_menuOverlays;
    QMutex                             *m_overlaysLock;
};

#endif // TORCVIDEOOVERLAY_H
