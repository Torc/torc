#ifndef TORCVIDEOINTERFACE_H
#define TORCVIDEOINTERFACE_H

// Qt
#include <QObject>

// Torc
#include "audiowrapper.h"
#include "torcvideoexport.h"
#include "videobuffers.h"
#include "torcplayer.h"

class VideoPlayer : public TorcPlayer
{
    Q_OBJECT

  public:
    VideoPlayer(QObject* Parent, int PlaybackFlags, int DecodeFlags);
    virtual ~VideoPlayer();

    virtual void    Refresh            (void);
    void*           GetAudio           (void);
    VideoBuffers*   Buffers            (void);

  protected:
    virtual void    Teardown           (void);

  protected:
    AudioWrapper   *m_audioWrapper;
    VideoBuffers    m_buffers;
};

#endif // TORCVIDEOINTERFACE_H
