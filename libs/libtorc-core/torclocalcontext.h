#ifndef TORCLOCALCONTEXT_H
#define TORCLOCALCONTEXT_H

// Qt
#include <QObject>
#include <QString>

// Torc
#include "torclocaldefs.h"
#include "torcconfig.h"
#include "torccoreexport.h"
#include "torcobservable.h"
#include "torccommandlineparser.h"

class TorcAdminObject;

class TORC_CORE_PUBLIC Torc
{
    Q_GADGET
    Q_ENUMS(Actions)

  public:
    enum Actions
    {
        None = 0,
        Custom,
        Exit,
        KeyPress,
        KeyRelease,
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
        VolumeUp,
        VolumeDown,
        Mute,
        Unmute,
        SetVolume,
        // storage
        DiskInserted = 4000,
        DiskRemoved,
        DiskCanBeSafelyRemoved,
        DiskEjected,
        // jumps
        ShowSearch = 5000,
        HideSearch,
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
    static qint16 Create      (TorcCommandLineParser* CommandLine);
    static void   TearDown    (void);
    static void   NotifyEvent (int Event);

    Q_INVOKABLE   QString    GetSetting    (const QString &Name, const QString &DefaultValue);
    Q_INVOKABLE   bool       GetSetting    (const QString &Name, const bool    &DefaultValue);
    Q_INVOKABLE   int        GetSetting    (const QString &Name, const int     &DefaultValue);
    Q_INVOKABLE   void       SetSetting    (const QString &Name, const QString &Value);
    Q_INVOKABLE   void       SetSetting    (const QString &Name, const bool    &Value);
    Q_INVOKABLE   void       SetSetting    (const QString &Name, const int     &Value);
    Q_INVOKABLE   QObject*   GetUIObject   (void);
    void          SetUIObject              (QObject* UI);
    void          CloseDatabaseConnections (void);

  private:
    TorcLocalContext(TorcCommandLineParser* CommandLine);
   ~TorcLocalContext();

    bool          Init(void);

  private:
    TorcLocalContextPriv *m_priv;
};

extern TORC_CORE_PUBLIC TorcLocalContext *gLocalContext;
extern TORC_CORE_PUBLIC QMutex           *gLocalContextLock;
extern TORC_CORE_PUBLIC QMutex           *gAVCodecLock;

#endif // TORCLOCALCONTEXT_H
