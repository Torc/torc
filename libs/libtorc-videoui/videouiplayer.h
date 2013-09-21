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

class VideoUIPlayer : public VideoPlayer
{
    Q_OBJECT

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
