#ifndef TORCAUDIOPLAYER_H
#define TORCAUDIOPLAYER_H

// Qt
#include <QObject>

// Torc
#include "torcplayer.h"
#include "http/torchttpservice.h"

class AudioWrapper;

class AudioPlayer : public TorcPlayer, public TorcHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",     "1.0.0")
    Q_CLASSINFO("Play",        "methods=PUT")
    Q_CLASSINFO("Pause",       "methods=PUT")
    Q_CLASSINFO("Unpause",     "methods=PUT")
    Q_CLASSINFO("TogglePause", "methods=PUT")
    Q_CLASSINFO("Stop",        "methods=PUT")
    Q_CLASSINFO("PlayMedia",   "methods=PUT")

  public:
    AudioPlayer(QObject* Parent, int PlaybackFlags, int DecodeFlags);
    virtual ~AudioPlayer();

    bool            PlayMedia          (const QString &URI, bool Paused);
    void*           GetAudio           (void);

  protected:
    void            Teardown           (void);

  private:
    AudioWrapper   *m_audioWrapper;
};

#endif // TORCPLAYER_H
