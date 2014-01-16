#ifndef TORCLOCALCONTEXT_H
#define TORCLOCALCONTEXT_H

// Qt
#include <QObject>
#include <QString>

// Torc
#include "torclocaldefs.h"
#include "torcconfig.h"
#include "torccoreexport.h"
#include "torcsetting.h"
#include "torclanguage.h"
#include "torcobservable.h"
#include "torccommandline.h"

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

class TorcAdminObject;

class TORC_CORE_PUBLIC Torc
{
    Q_GADGET
    Q_FLAGS(ApplicationFlags)
    Q_ENUMS(Actions)
    Q_ENUMS(MessageTypes)
    Q_ENUMS(MessageDestinations)
    Q_ENUMS(MessageTimeouts)

  public:
    enum ApplicationFlag
    {
        NoFlags     = (0 << 0),
        Database    = (1 << 0),
        Client      = (1 << 1),
        Server      = (1 << 2),
        Power       = (1 << 3),
        Storage     = (1 << 4),
        USB         = (1 << 5),
        Network     = (1 << 6),
        AdminThread = (1 << 7)
    };

    Q_DECLARE_FLAGS(ApplicationFlags, ApplicationFlag);

    enum MessageTypes
    {
        GenericError,
        CriticalError,
        GenericWarning,
        ExternalMessage,
        InternalMessage
    };

    enum MessageDestinations
    {
        Internal  = 0x01,
        Local     = 0x02,
        Broadcast = 0x04
    };

    enum MessageTimeouts
    {
        DefaultTimeout,
        ShortTimeout,
        LongTimeout,
        Acknowledge
    };

    enum Actions
    {
        None = 0,
        Custom,
        Exit,
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
        DisplayDeviceReset,
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
        BackSpace,
        // Media playback
        Stop = 3000,
        Play,
        Pause,
        Unpause,
        TogglePlayPause,
        VolumeUp,
        VolumeDown,
        Mute,
        Unmute,
        ToggleMute,
        SetVolume,
        JumpForwardSmall,
        JumpForwardLarge,
        JumpForwardTrack,
        JumpToEnd,
        JumpBackwardSmall,
        JumpBackwardLarge,
        JumpBackwardTrack,
        JumpToStart,
        PlayMedia,
        EnableHighQualityScaling,
        DisableHighQualityScaling,
        ToggleHighQualityScaling,
        IncreaseBrightness,
        IncreaseContrast,
        IncreaseSaturation,
        IncreaseHue,
        DecreaseBrightness,
        DecreaseContrast,
        DecreaseSaturation,
        DecreaseHue,
        SetBrightness,
        SetContrast,
        SetSaturation,
        SetHue,
        // storage
        DiskInserted = 4000,
        DiskRemoved,
        DiskCanBeSafelyRemoved,
        DiskEjected,
        EnableStorage,
        DisableStorage,
        StorageEnabled,
        StorageDisabled,
        // jumps
        ShowSearch = 5000,
        HideSearch,
        // network
        NetworkEnabled = 6000,
        NetworkDisabled,
        NetworkAvailable,
        NetworkUnavailable,
        NetworkChanged,
        NetworkHostNamesChanged,
        // appearance
        EnableStudioLevels = 7000,
        DisableStudioLevels,
        ToggleStudioLevels,
        // USB
        USBRescan = 8000,
        USBDeviceAdded,
        USBDeviceRemoved,
        // users
        ChangeUser = 9000,
        UserChanged,
        // service discovery
        ServiceDiscovered = 10000,
        ServiceWentAway,
        // media discovery
        MediaAdded = 11000,
        MediaRemoved,
        MediaChanged,
        // end of predefined
        MaxTorc = 60000,
        // plugins etc
        MaxUser = 65535
    };

    static QString ActionToString(enum Actions Action);
    static int     StringToAction(const QString &Action);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Torc::ApplicationFlags);

class TorcLocalContextPriv;
class QMutex;
class QReadWriteLock;

class TORC_CORE_PUBLIC TorcLocalContext : public QObject, public TorcObservable
{
    Q_OBJECT

  public:
    static qint16 Create      (TorcCommandLine* CommandLine, Torc::ApplicationFlags ApplicationFlags);
    static void   TearDown    (void);

    Q_INVOKABLE static void  NotifyEvent   (int Event);
    Q_INVOKABLE static void  UserMessage   (int Type, int Destination, int Timeout,
                                            const QString &Header, const QString &Body,
                                            QString Uuid = QString());
    Q_INVOKABLE   QString    GetSetting    (const QString &Name, const QString &DefaultValue);
    Q_INVOKABLE   bool       GetSetting    (const QString &Name, const bool    &DefaultValue);
    Q_INVOKABLE   int        GetSetting    (const QString &Name, const int     &DefaultValue);
    Q_INVOKABLE   void       SetSetting    (const QString &Name, const QString &Value);
    Q_INVOKABLE   void       SetSetting    (const QString &Name, const bool    &Value);
    Q_INVOKABLE   void       SetSetting    (const QString &Name, const int     &Value);
    Q_INVOKABLE   QString    GetPreference (const QString &Name, const QString &DefaultValue);
    Q_INVOKABLE   bool       GetPreference (const QString &Name, const bool    &DefaultValue);
    Q_INVOKABLE   int        GetPreference (const QString &Name, const int     &DefaultValue);
    Q_INVOKABLE   void       SetPreference (const QString &Name, const QString &Value);
    Q_INVOKABLE   void       SetPreference (const QString &Name, const bool    &Value);
    Q_INVOKABLE   void       SetPreference (const QString &Name, const int     &Value);
    Q_INVOKABLE   bool       FlagIsSet     (Torc::ApplicationFlag Flag);
    Q_INVOKABLE   QObject*   GetUIObject   (void);
    Q_INVOKABLE   QString    GetUuid       (void);
    Q_INVOKABLE   TorcSetting* GetRootSetting (void);
    Q_INVOKABLE   qint64     GetStartTime  (void);
    Q_INVOKABLE   int        GetPriority   (void);

    QLocale::Language        GetLanguage   (void);
    void                     SetUIObject   (QObject* UI);
    void                     CloseDatabaseConnections (void);

  public slots:
    void                     RegisterQThread          (void);
    void                     DeregisterQThread        (void);


  private:
    TorcLocalContext(TorcCommandLine* CommandLine, Torc::ApplicationFlags ApplicationFlags);
   ~TorcLocalContext();

    bool          Init(void);

  private:
    TorcLocalContextPriv *m_priv;
};

extern TORC_CORE_PUBLIC TorcLocalContext *gLocalContext;
extern TORC_CORE_PUBLIC QMutex           *gAVCodecLock;
extern TORC_CORE_PUBLIC TorcSetting      *gRootSetting;

#endif // TORCLOCALCONTEXT_H
