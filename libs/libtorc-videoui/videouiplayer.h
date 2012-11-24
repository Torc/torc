#ifndef VIDEOUIPLAYER_H
#define VIDEOUIPLAYER_H

// Qt
#include <QObject>

// Torc
#include "torcvideouiexport.h"
#include "http/torchttpservice.h"
#include "videorenderer.h"
#include "videoplayer.h"

class VideoUIPlayer : public VideoPlayer, public TorcHTTPService
{
    Q_OBJECT

  public:
    VideoUIPlayer(QObject* Parent, int PlaybackFlags, int DecodeFlags);
    virtual ~VideoUIPlayer();

    void            Refresh    (void);

  public slots:
    void            PlayerStateChanged (TorcPlayer::PlayerState NewState);

  protected:
    virtual void    Teardown   (void);

  protected:
    VideoRenderer*  m_render;
};

#endif // VIDEOUIPLAYER_H
