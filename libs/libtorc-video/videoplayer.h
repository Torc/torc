#ifndef TORCVIDEOINTERFACE_H
#define TORCVIDEOINTERFACE_H

// Qt
#include <QObject>

// Torc
#include "audiowrapper.h"
#include "torcvideoexport.h"
#include "torcplayer.h"

class VideoPlayer : public TorcPlayer
{
    Q_OBJECT

  public:
    VideoPlayer(QObject* Parent, int PlaybackFlags, int DecodeFlags);
    virtual ~VideoPlayer();

    void*           GetAudio           (void);

  protected:
    void            Teardown           (void);

  private:
    AudioWrapper   *m_audioWrapper;
};

#endif // TORCVIDEOINTERFACE_H
