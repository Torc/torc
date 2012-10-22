#ifndef TORCLOCALCONTEXT_H
#define TORCLOCALCONTEXT_H

// Qt
#include <QObject>
#include <QString>

// Torc
#include "torclocaldefs.h"
#include "torcconfig.h"
#include "torccoreexport.h"
#include "torclanguage.h"
#include "torcobservable.h"
#include "torccommandlineparser.h"

class TorcAdminObject;

class TORC_CORE_PUBLIC Torc
{
    Q_GADGET
    Q_ENUMS(Actions)

  public:
    enum ApplicationFlags
    {
        NoFlags     = (0 << 0),
        Database    = (1 << 0),
        Client      = (1 << 1),
        Server      = (1 << 2),
        Power       = (1 << 3),
        Storage     = (1 << 4),
        USB         = (1 << 5),
        Network     = (1 << 6)
    };

    typedef enum MessageTypes
    {
        GenericError,
        CriticalError,
        GenericWarning,
        ExternalMessage,
        InternalMessage
    } MessageType;

    typedef enum MessageDestinations
    {
        Internal  = (0 << 0),
        Local     = (1 << 0),
        Broadcast = (1 << 1)
    } MessageDestination;

    typedef enum MessageTimeouts
    {
        DefaultTimeout,
        ShortTimeout,
        LongTimeout,
        Acknowledge
    } MessageTimeout;

    enum Actions
    {
        None = 0,
        Custom,
        Exit,
        KeyPress,
        KeyRelease,
        Message,
        // Power management
        Shutdown = 1000,
        Suspend,
        Hibernate,
        Restart,
        ShuttingDown,
        Suspending,
        Hibernating,
        Restarting,
        WokeUp,
        LowBattery,
        StartScreensaver,
        StopScreensaver,
        PokeScreensaver,
        ScreensaverStarting,
        ScreensaverStopping,
        MakeHDMISourceActive,
        MakeHDMISourceInactive,
        HDMISourceActivated,
        HDMISourceDeactivated,
        // General UI
        Pushed = 2000,
        Released,
        Select = Released,
        Escape,
        Menu,
        Info,
        Left,
        Right,
        Up,
        Down,
        PageUp,
        PageDown,
        Top,
        Bottom,
        Zero,
        One,
        Two,
        Three,
        Four,
        Five,
        Six,
        Seven,
        Eight,
        Nine,
        // Media playback
        Stop = 3000,
        Play,
        Pause,
        Unpause,
        TogglePause,
        VolumeUp,
        VolumeDown,
        Mute,
        Unmute,
        SetVolume,
        JumpForwardSmall,
        JumpForwardBig,
        JumpForwardTrack,
        JumpToEnd,
        JumpBackwardSmall,
        JumpBackwardLarge,
        JumpBackwardTrack,
        JumpToStart,
        PlayMedia,
        // storage
        DiskInserted = 4000,
        DiskRemoved,
        DiskCanBeSafelyRemoved,
        DiskEjected,
        // jumps
        ShowSearch = 5000,
        HideSearch,
        // network
        EnableNetwork,
        DisableNetwork,
        NetworkEnabled,
        NetworkDisabled,
        NetworkAvailable,
        NetworkUnavailable,
        // end of predefined
        MaxTorc = 60000,
        // plugins etc
        MaxUser = 65535
    };

    static QString ActionToString(enum Actions Action);
};

class TorcLocalContextPriv;
class QMutex;
class QReadWriteLock;

class TORC_CORE_PUBLIC TorcLocalContext : public QObject, public TorcObservable
{
    Q_OBJECT

  public:
    static qint16 Create      (TorcCommandLineParser* CommandLine, int ApplicationFlags);
    static void   TearDown    (void);
    static void   NotifyEvent (int Event);
    static void   SendMessage (Torc::MessageType Type, Torc::MessageDestination Destination,
                               Torc::MessageTimeout Timeout, QString Uuid,
                               const QString &Header, const QString &Body);

    Q_INVOKABLE   QString    GetSetting    (const QString &Name, const QString &DefaultValue);
    Q_INVOKABLE   bool       GetSetting    (const QString &Name, const bool    &DefaultValue);
    Q_INVOKABLE   int        GetSetting    (const QString &Name, const int     &DefaultValue);
    Q_INVOKABLE   void       SetSetting    (const QString &Name, const QString &Value);
    Q_INVOKABLE   void       SetSetting    (const QString &Name, const bool    &Value);
    Q_INVOKABLE   void       SetSetting    (const QString &Name, const int     &Value);
    Q_INVOKABLE   QObject*   GetUIObject   (void);
    QLocale::Language        GetLanguage   (void);
    void          SetUIObject              (QObject* UI);
    void          CloseDatabaseConnections (void);
    bool          GetFlag                  (int Flag);

  private:
    TorcLocalContext(TorcCommandLineParser* CommandLine, int ApplicationFlags);
   ~TorcLocalContext();

    bool          Init(void);

  private:
    TorcLocalContextPriv *m_priv;
};

extern TORC_CORE_PUBLIC TorcLocalContext *gLocalContext;
extern TORC_CORE_PUBLIC QMutex           *gLocalContextLock;
extern TORC_CORE_PUBLIC QMutex           *gAVCodecLock;

#endif // TORCLOCALCONTEXT_H
