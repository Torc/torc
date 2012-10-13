#ifndef TORCAUDIOPLAYER_H
#define TORCAUDIOPLAYER_H

// Qt
#include <QObject>

// Torc
#include "torcplayer.h"

class AudioWrapper;

class AudioPlayer : public TorcPlayer
{
    Q_OBJECT

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
