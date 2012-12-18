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

  public:
    static void        Initialise    (void);

  public:
    VideoUIPlayer(QObject* Parent, int PlaybackFlags, int DecodeFlags);
    virtual ~VideoUIPlayer();

    bool               Refresh       (quint64 TimeNow);
    void               Render        (quint64 TimeNow);
    void               Reset         (void);

  protected:
    virtual void       Teardown      (void);

  protected:
    VideoRenderer     *m_render;
    VideoColourSpace  *m_colourSpace;
};

#endif // VIDEOUIPLAYER_H
