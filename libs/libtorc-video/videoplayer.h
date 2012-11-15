#ifndef TORCVIDEOINTERFACE_H
#define TORCVIDEOINTERFACE_H

// Qt
#include <QObject>

// Torc
#include "audiowrapper.h"
#include "torcvideoexport.h"
#include "http/torchttpservice.h"
#include "videobuffers.h"
#include "torcplayer.h"

class VideoPlayer : public TorcPlayer, public TorcHTTPService
{
    Q_OBJECT

  public:
    VideoPlayer(QObject* Parent, int PlaybackFlags, int DecodeFlags);
    virtual ~VideoPlayer();

    void            Refresh            (void);
    void*           GetAudio           (void);
    VideoBuffers*   Buffers            (void);

  protected:
    void            Teardown           (void);

  private:
    AudioWrapper   *m_audioWrapper;
    VideoBuffers    m_buffers;
};

#endif // TORCVIDEOINTERFACE_H
