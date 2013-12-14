#ifndef TORCOMXVIDEOPLAYER_H
#define TORCOMXVIDEOPLAYER_H

// Torc
#include "torcsgvideoplayer.h"

class TorcOMXVideoPlayer : public TorcSGVideoPlayer
{
  public:
    TorcOMXVideoPlayer(QObject* Parent, int PlaybackFlags, int DecodeFlags);
    virtual ~TorcOMXVideoPlayer();

    bool           Refresh                (quint64 TimeNow, const QSizeF &Size, bool Visible);
    void           Render                 (quint64 TimeNow);
};

#endif // TORCOMXVIDEOPLAYER_H
