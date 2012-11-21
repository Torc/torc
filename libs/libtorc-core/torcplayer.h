#ifndef TORCPLAYER_H
#define TORCPLAYER_H

// Qt
#include <QObject>
#include <QString>

// Torc
#include "torccoreexport.h"

class TorcDecoder;

class TORC_CORE_PUBLIC TorcPlayer : public QObject
{
    Q_OBJECT

  public:
    enum PlayerFlags
    {
        NoFlags      = (0 << 0),
        AudioMuted   = (1 << 0),
        AudioDummy   = (1 << 1),
        UserFacing   = (1 << 2)
    };

    typedef enum PlayerState
    {
        Errored = -1,
        None    = 0,
        Opening,
        Paused,
        Starting,
        Playing,
        Searching,
        Pausing,
        Stopping,
        Stopped
    } PlayerState;

  public:
    static TorcPlayer* Create(QObject *Parent, int PlaybackFlags, int DecoderFlags);
    static QString     StateToString(PlayerState State);

    virtual ~TorcPlayer();

    virtual void    Refresh            (void);
    void            Reset              (void);
    bool            HandleEvent        (QEvent *Event);
    bool            HandleAction       (int Action);
    bool            IsSwitching        (void);
    PlayerState     GetState           (void);
    PlayerState     GetNextState       (void);

    virtual void*   GetAudio           (void) = 0;
    void            SendUserMessage    (const QString &Message);
    int             GetPlayerFlags     (void);
    int             GetDecoderFlags    (void);

  public slots:
    virtual bool    PlayMedia          (const QString &URI, bool Paused);
    qreal           GetSpeed           (void);
    bool            Play               (void);
    bool            Stop               (void);
    bool            Pause              (void);
    bool            Unpause            (void);
    bool            TogglePause        (void);

  signals:
    void            StateChanged       (TorcPlayer::PlayerState NewState);

  protected:
    TorcPlayer(QObject *Parent, int PlaybackFlags, int DecoderFlags);
    virtual void    Teardown           (void);

  protected:
    void            SetState           (PlayerState NewState);
    void            StartTimer         (int &Timer, int Timeout);
    void            KillTimer          (int &Timer);
    void            DestroyNextDecoder (void);
    void            DestroyOldDecoder  (void);
    bool            event              (QEvent* Event);

  protected:
    int             m_playerFlags;
    int             m_decoderFlags;
    QString         m_uri;
    PlayerState     m_state;
    PlayerState     m_nextState;
    qreal           m_speed;
    int             m_pauseTimer;
    int             m_playTimer;
    int             m_stopTimer;
    int             m_refreshTimer;

    TorcDecoder    *m_decoder;

    bool            m_switching;
    QString         m_nextUri;
    TorcDecoder    *m_nextDecoder;
    bool            m_nextDecoderPlay;
    int             m_nextDecoderStartTimer;

    TorcDecoder    *m_oldDecoder;
    int             m_oldDecoderStopTimer;

};

class TORC_CORE_PUBLIC PlayerFactory
{
  public:
    PlayerFactory();
    virtual ~PlayerFactory();
    static PlayerFactory* GetPlayerFactory  (void);
    PlayerFactory*        NextFactory       (void) const;
    virtual TorcPlayer*   Create            (QObject *Parent, int PlaybackFlags, int DecoderFlags) = 0;

  protected:
    static PlayerFactory* gPlayerFactory;
    PlayerFactory*        nextPlayerFactory;
};

class TORC_CORE_PUBLIC  TorcPlayerInterface
{
  public:
    TorcPlayerInterface(bool Standalone);
    virtual ~TorcPlayerInterface();

    virtual bool   InitialisePlayer   (void) = 0;
    virtual bool   HandlePlayerAction (int   Action);
    virtual bool   HandleEvent        (QEvent *Event);
    virtual bool   PlayMedia          (bool Paused);

    void           SetURI             (const QString &URI);

  protected:
    QString        m_uri;
    TorcPlayer    *m_player;
    bool           m_standalone;
    bool           m_pausedForSuspend;
    bool           m_pausedForInactiveSource;
};

#endif // TORCPLAYER_H
