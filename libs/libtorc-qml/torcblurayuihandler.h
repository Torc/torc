#ifndef TORCBLURAYUIHANDLER_H
#define TORCBLURAYUIHANDLER_H

// Torc
#include "torcqmlexport.h"
#include "torcblurayhandler.h"

class TorcBlurayUIBuffer;

class TORC_QML_PUBLIC TorcBlurayUIHandler : public TorcBlurayHandler
{
  public:
    TorcBlurayUIHandler(TorcBlurayBuffer *Parent, const QString &Path, int *Abort);
    ~TorcBlurayUIHandler();

    bool                Open                 (void);
    void                Close                (void);
    int                 Read                 (quint8 *Buffer, qint32 BufferSize);
    AVInputFormat*      GetAVInputFormat     (void);
    void                SetIgnoreWaits       (bool Ignore);

    void                HandleOverlay        (const struct bd_overlay_s * const Overlay);
    void                HandleARGBOverlay    (const struct bd_argb_overlay_s * const Overlay);
    bool                MouseClicked         (qint64 Pts, quint16 X, quint16 Y);
    bool                MouseMoved           (qint64 Pts, quint16 X, quint16 Y);

  protected:
    int                 HandleBDEvent        (BD_EVENT &Event);
    void                ClearOverlays        (void);
    void                ClearARGBOverlays    (void);

  protected:
    VideoPlayer              *m_player;
    VideoDecoder             *m_decoder;
    qint32                    m_currentPlayList;
    quint32                   m_currentPlayItem;
    bool                      m_ignoreWaits;
    bool                      m_endOfTitle;
    QList<bd_overlay_s*>      m_overlays[2];
    QList<bd_argb_overlay_s*> m_argbOverlays[2];
};

#endif // TORCBLURAYUIHANDLER_H
