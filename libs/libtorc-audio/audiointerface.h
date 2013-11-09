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
    Q_CLASSINFO("Version",    "1.0.0")

  public:
    AudioInterface(bool Standalone);
    virtual ~AudioInterface();

    bool  event              (QEvent *Event);
    bool  InitialisePlayer   (void);

  public slots:
    void  SubscriberDeleted  (QObject *Subscriber);
    void  PlayerStateChanged (TorcPlayer::PlayerState NewState);
};

#endif // TORCPLAYERINTERFACE_H
