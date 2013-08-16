#ifndef VIDEOUIPLAYER_H
#define VIDEOUIPLAYER_H

// Qt
#include <QObject>

// Torc
#include "torcvideouiexport.h"
#include "http/torchttpservice.h"
#include "videorenderer.h"
#include "videoplayer.h"

class VideoColourSpace;

class VideoUIPlayer : public VideoPlayer, public TorcHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Play",        "methods=PUT")
    Q_CLASSINFO("Pause",       "methods=PUT")
    Q_CLASSINFO("Unpause",     "methods=PUT")
    Q_CLASSINFO("TogglePause", "methods=PUT")
    Q_CLASSINFO("Stop",        "methods=PUT")
    Q_CLASSINFO("PlayMedia",   "methods=PUT")

  public:
    static void        Initialise    (void);

  public:
    VideoUIPlayer(QObject* Parent, int PlaybackFlags, int DecodeFlags);
    virtual ~VideoUIPlayer();

    bool               Refresh                (quint64 TimeNow, const QSizeF &Size, bool Visible);
    void               Render                 (quint64 TimeNow);
    void               Reset                  (void);
    bool               HandleAction           (int Action);
    QVariant           GetProperty            (PlayerProperty Property);
    void               SetProperty            (PlayerProperty Property, QVariant Value);

  protected:
    virtual void       Teardown               (void);

  protected:
    VideoRenderer     *m_render;
    VideoColourSpace  *m_colourSpace;
    int                m_manualAVSyncAdjustment;
    VideoFrame        *m_currentFrame;
};

#endif // VIDEOUIPLAYER_H
