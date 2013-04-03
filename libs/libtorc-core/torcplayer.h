#ifndef TORCPLAYER_H
#define TORCPLAYER_H

// Qt
#include <QSet>
#include <QSize>
#include <QObject>
#include <QString>
#include <QVariant>

// Torc
#include "torccoreexport.h"

class TorcDecoder;

class TORC_CORE_PUBLIC TorcPlayer : public QObject
{
    Q_OBJECT
    Q_ENUMS(PlayerProperty)

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

    enum PlayerProperty
    {
        Unknown = 0,
        // appearance
        Brightness = 0x1000,
        Contrast,
        Saturation,
        Hue,
        HQScaling,
        BasicDeinterlacing,
        MediumDeinterlacing,
        AdvancedDeinterlacing,
        // state
        Speed = 0x2000
    };

  public:
    static TorcPlayer* Create              (QObject *Parent, int PlaybackFlags, int DecoderFlags);
    static QString     StateToString       (PlayerState State);
    static QString     PropertyToString    (PlayerProperty Property);
    static PlayerProperty StringToProperty (const QString &Property);

    virtual ~TorcPlayer();

    virtual bool    Refresh                (quint64 TimeNow, const QSizeF &Size);
    virtual void    Render                 (quint64 TimeNow);
    virtual QVariant GetProperty           (PlayerProperty Property);
    virtual void    SetProperty            (PlayerProperty Property, QVariant Value);
    bool            IsPropertyAvailable    (PlayerProperty Property);

    void            Reset                  (void);
    bool            HandleEvent            (QEvent *Event);
    virtual bool    HandleAction           (int Action);
    bool            IsSwitching            (void);
    PlayerState     GetState               (void);
    PlayerState     GetNextState           (void);

    virtual void*   GetAudio               (void) = 0;
    void            SendUserMessage        (const QString &Message);
    int             GetPlayerFlags         (void);
    int             GetDecoderFlags        (void);

  public slots:
    virtual bool    PlayMedia              (const QString &URI, bool StartPaused);
    bool            Play                   (void);
    bool            Stop                   (void);
    bool            Pause                  (void);
    bool            Unpause                (void);
    bool            TogglePause            (void);
    void            SetPropertyAvailable   (TorcPlayer::PlayerProperty Property);
    void            SetPropertyUnavailable (TorcPlayer::PlayerProperty Property);


  signals:
    void            StateChanged           (TorcPlayer::PlayerState NewState);
    void            PropertyAvailable      (TorcPlayer::PlayerProperty Property);
    void            PropertyUnavailable    (TorcPlayer::PlayerProperty Property);

  protected:
    TorcPlayer(QObject *Parent, int PlaybackFlags, int DecoderFlags);
    virtual void    Teardown               (void);

  protected:
    void            SetState               (PlayerState NewState);
    void            StartTimer             (int &Timer, int Timeout);
    void            KillTimer              (int &Timer);
    void            DestroyNextDecoder     (void);
    void            DestroyOldDecoder      (void);
    bool            event                  (QEvent* Event);

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

    QSet<PlayerProperty> m_supportedProperties;

};

class TORC_CORE_PUBLIC PlayerFactory
{
  public:
    PlayerFactory();
    virtual ~PlayerFactory();
    static PlayerFactory* GetPlayerFactory  (void);
    PlayerFactory*        NextFactory       (void) const;
    virtual void          Score             (QObject *Parent, int PlaybackFlags, int DecoderFlags, int &Score) = 0;
    virtual TorcPlayer*   Create            (QObject *Parent, int PlaybackFlags, int DecoderFlags, int &Score) = 0;

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
