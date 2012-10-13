#ifndef TORCPLAYERINTERFACE_H
#define TORCPLAYERINTERFACE_H

// Qt
#include <QObject>

// Torc
#include "torcplayer.h"
#include "torcaudioexport.h"

class TORC_AUDIO_PUBLIC AudioInterface : public QObject, public TorcPlayerInterface
{
    Q_OBJECT

  public:
    AudioInterface(QObject *Parent, bool Standalone);
    virtual ~AudioInterface();

    bool  event              (QEvent *Event);
    bool  InitialisePlayer   (void);

  public slots:
    void  PlayerStateChanged (TorcPlayer::PlayerState NewState);
};

#endif // TORCPLAYERINTERFACE_H
