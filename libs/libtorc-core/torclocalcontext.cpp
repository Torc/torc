/* Class TorcLocalContext
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Std
#include <signal.h>

// Qt
#include <QCoreApplication>
#include <QReadWriteLock>
#include <QSqlDatabase>
#include <QThreadPool>
#include <QMutex>
#include <QUuid>
#include <QDir>
#include <QMetaEnum>

// Torc
#include "torcevent.h"
#include "torcloggingimp.h"
#include "torclogging.h"
#include "torcexitcodes.h"
#include "torcsqlitedb.h"
#include "torcadminthread.h"
#include "torcpower.h"
#include "torcstoragedevice.h"
#include "torclocalcontext.h"
#include "torcdirectories.h"
#include "version.h"

TorcLocalContext *gLocalContext = NULL;
QMutex*       gLocalContextLock = new QMutex(QMutex::Recursive);
QMutex*            gAVCodecLock = new QMutex(QMutex::Recursive);

static void ExitHandler(int Sig)
{
    signal(SIGINT, SIG_DFL);
    LOG(VB_GENERAL, LOG_INFO, QString("Received %1")
        .arg(Sig == SIGINT ? "SIGINT" : "SIGTERM"));

    TorcLocalContext::NotifyEvent(Torc::Exit);
}

class TorcLocalContextPriv
{
  public:
    TorcLocalContextPriv(int ApplicatinFlags);
   ~TorcLocalContextPriv();

    bool    Init                 (void);
    QString GetSetting           (const QString &Name, const QString &DefaultValue);
    void    SetSetting           (const QString &Name, const QString &Value);
    void    PrintSessionSettings (void);

    int                   m_flags;
    TorcSQLiteDB         *m_sqliteDB;
    QMap<QString,QString> m_localSettings;
    QMap<QString,QString> m_sessionSettings;
    QReadWriteLock       *m_localSettingsLock;
    QObject              *m_UIObject;
    TorcAdminThread      *m_adminThread;
    TorcLanguage          m_language;
};

TorcLocalContextPriv::TorcLocalContextPriv(int ApplicationFlags)
  : m_flags(ApplicationFlags),
    m_sqliteDB(NULL),
    m_localSettingsLock(new QReadWriteLock(QReadWriteLock::Recursive)),
    m_UIObject(NULL),
    m_adminThread(NULL)
{
}

TorcLocalContextPriv::~TorcLocalContextPriv()
{
    // close and cleanup the admin thread
    if (m_adminThread)
    {
        m_adminThread->quit();
        m_adminThread->wait();
        delete m_adminThread;
        m_adminThread = NULL;
    }

    // wait for threads to exit
    QThreadPool::globalInstance()->waitForDone();

    // delete the database connection(s)
    delete m_sqliteDB;
    m_sqliteDB = NULL;

    // delete settings lock
    delete m_localSettingsLock;
    m_localSettingsLock = NULL;
}

bool TorcLocalContextPriv::Init(void)
{
    // Create the configuration directory
    QString configdir = GetTorcConfigDir();

    QDir dir(configdir);
    if (!dir.exists())
    {
        if (!dir.mkdir(configdir))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to create config directory ('%1')")
                .arg(configdir));
            return false;
        }
    }

    // Open the local database
    if (m_flags & Torc::Database)
    {
        QString dbname = configdir + "/torc-settings.sqlite";
        m_sqliteDB = new TorcSQLiteDB(dbname);

        if (!m_sqliteDB || (m_sqliteDB && !m_sqliteDB->IsValid()))
            return false;

        // Load the settings
        {
            QWriteLocker locker(m_localSettingsLock);
            m_sqliteDB->LoadSettings(m_localSettings);
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, "Running without database");
    }

    // don't expire threads
    QThreadPool::globalInstance()->setExpiryTimeout(-1);

    // Increase the thread count
    int ideal = QThreadPool::globalInstance()->maxThreadCount();
    int want  = ideal * 2;
    LOG(VB_GENERAL, LOG_INFO, QString("Setting thread pool size to %1 (was %2)")
        .arg(want).arg(ideal));
    QThreadPool::globalInstance()->setMaxThreadCount(want);

    // Bump the UI thread priority
    QThread::currentThread()->setPriority(QThread::TimeCriticalPriority);

    // Qt version?
    LOG(VB_GENERAL, LOG_INFO, QString("Qt runtime version '%1' (compiled with '%2')")
        .arg(qVersion()).arg(QT_VERSION_STR));

    // Load language preferences
    m_language.LoadPreferences();

    // create an admin thread (and associated objects)
    m_adminThread = new TorcAdminThread();
    m_adminThread->start();

    return true;
}

QString TorcLocalContextPriv::GetSetting(const QString &Name,
                                         const QString &DefaultValue)
{
    {
        QReadLocker locker(m_localSettingsLock);

        if (m_sessionSettings.contains(Name))
            return m_sessionSettings.value(Name);

        if (m_localSettings.contains(Name))
            return m_localSettings.value(Name);
    }

    SetSetting(Name, DefaultValue);
    return DefaultValue;
}

void TorcLocalContextPriv::SetSetting(const QString &Name, const QString &Value)
{
    QWriteLocker locker(m_localSettingsLock);
    if (m_sqliteDB)
        m_sqliteDB->SetSetting(Name, Value);
    m_localSettings[Name] = Value;
}

void TorcLocalContextPriv::PrintSessionSettings(void)
{
    QReadLocker locker(m_localSettingsLock);
    QMapIterator<QString,QString> it(m_sessionSettings);
    while (it.hasNext())
    {
        it.next();
        LOG(VB_GENERAL, LOG_NOTICE,
            QString("Temp setting: '%1'='%2'")
            .arg(it.key()).arg(it.value()));
    }
}

QString Torc::ActionToString(Actions Action)
{
    const QMetaObject &mo = Torc::staticMetaObject;
    int enum_index        = mo.indexOfEnumerator("Actions");
    QMetaEnum metaEnum    = mo.enumerator(enum_index);
    return metaEnum.valueToKey(Action);
}

int Torc::StringToAction(const QString &Action)
{
    const QMetaObject &mo = Torc::staticMetaObject;
    int enum_index        = mo.indexOfEnumerator("Actions");
    QMetaEnum metaEnum    = mo.enumerator(enum_index);
    return metaEnum.keyToValue(Action.toLatin1());
}

qint16 TorcLocalContext::Create(TorcCommandLineParser* CommandLine, int ApplicationFlags)
{
    QMutexLocker locker(gLocalContextLock);
    if (gLocalContext)
        return GENERIC_EXIT_OK;

    gLocalContext = new TorcLocalContext(CommandLine, ApplicationFlags);
    if (gLocalContext && gLocalContext->Init())
        return GENERIC_EXIT_OK;

    TearDown();
    return GENERIC_EXIT_NO_CONTEXT;
}

void TorcLocalContext::TearDown(void)
{
    QMutexLocker locker(gLocalContextLock);
    delete gLocalContext;
    gLocalContext = NULL;
}

void TorcLocalContext::NotifyEvent(int Event)
{
    QMutexLocker locker(gLocalContextLock);
    TorcEvent event(Event);
    if (gLocalContext)
        gLocalContext->Notify(event);
}

#define MESSAGE_TIMEOUT_DEFAULT 3000
#define MESSAGE_TIMEOUT_SHORT   1000
#define MESSAGE_TIMEOUT_LONG    10000

void TorcLocalContext::SendMessage(int Type, int Destination, int Timeout,
                                   const QString &Header, const QString &Body,
                                   QString Uuid)
{
    if (Body.isEmpty())
        return;

    int timeout = -1;
    switch (Timeout)
    {
        case Torc::DefaultTimeout:
            timeout = MESSAGE_TIMEOUT_DEFAULT;
            break;
        case Torc::ShortTimeout:
            timeout = MESSAGE_TIMEOUT_SHORT;
            break;
        case Torc::LongTimeout:
            timeout = MESSAGE_TIMEOUT_LONG;
            break;
        default:
            break;
    }

    if (Uuid.isEmpty())
    {
        QUuid dummy = QUuid::createUuid();
        Uuid = dummy.toString();
    }

    QVariantMap data;
    data.insert("type", Type);
    data.insert("destination", Destination);
    data.insert("timeout", timeout);
    data.insert("uuid", Uuid);
    data.insert("header", Header);
    data.insert("body", Body);
    TorcEvent event(Torc::Message, data);
    gLocalContext->Notify(event);
}

TorcLocalContext::TorcLocalContext(TorcCommandLineParser* CommandLine, int ApplicationFlags)
  : QObject(),
    m_priv(new TorcLocalContextPriv(ApplicationFlags))
{
    setObjectName("LocalContext");

    // Handle signals gracefully
    signal(SIGINT,  ExitHandler);
    signal(SIGTERM, ExitHandler);

    // Get overridden settings
    QMap<QString,QString> settings = CommandLine->GetSettingsOverride();
    {
        QWriteLocker locker(m_priv->m_localSettingsLock);
        QMapIterator<QString,QString> it(settings);
        while (it.hasNext())
        {
            it.next();
            m_priv->m_sessionSettings.insert(it.key(), it.value());
        }
    }

    // Initialise local directories
    InitialiseTorcDirectories();

    // Start logging at the first opportunity
    QString logfile(GetTorcConfigDir() + "/" +
                    QCoreApplication::applicationName() + ".log");

    ParseVerboseArgument("general");
    if (CommandLine->ToBool("verbose"))
        ParseVerboseArgument(CommandLine->ToString("verbose"));

    gVerboseMask |= VB_STDIO | VB_FLUSH;

    int quiet = CommandLine->ToUInt("quiet");
    if (quiet > 1)
    {
        gVerboseMask = VB_NONE | VB_FLUSH;
        ParseVerboseArgument("none");
    }

    StartLogging(logfile, 0, quiet, CommandLine->GetLoggingLevel(), false);

    // Debug the config directory
    LOG(VB_GENERAL, LOG_INFO, QString("Dir: Using '%1'")
        .arg(GetTorcConfigDir()));

    // Version info
    LOG(VB_GENERAL, LOG_CRIT,
        QString("%1 version: %2 [%3] www.torcdvr.org")
        .arg(QCoreApplication::applicationName())
        .arg(TORC_SOURCE_PATH).arg(TORC_SOURCE_VERSION));
    LOG(VB_GENERAL, LOG_NOTICE,
        QString("Enabled verbose msgs: %1").arg(gVerboseString));

    // Debug the overide settings
    m_priv->PrintSessionSettings();
}

TorcLocalContext::~TorcLocalContext()
{
    delete m_priv;
    m_priv = NULL;

    StopLogging();
}

bool TorcLocalContext::Init(void)
{
    if (!m_priv->Init())
        return false;

    return true;
}

QString TorcLocalContext::GetSetting(const QString &Name, const QString &DefaultValue)
{
    return m_priv->GetSetting(Name, DefaultValue);
}

bool TorcLocalContext::GetSetting(const QString &Name, const bool &DefaultValue)
{
    QString value = GetSetting(Name, DefaultValue ? QString("1") : QString("0"));
    return value.trimmed() == "1";
}

int TorcLocalContext::GetSetting(const QString &Name, const int &DefaultValue)
{
    QString value = GetSetting(Name, QString::number(DefaultValue));
    return value.toInt();
}

void TorcLocalContext::SetSetting(const QString &Name, const QString &Value)
{
    m_priv->SetSetting(Name, Value);
}

void TorcLocalContext::SetSetting(const QString &Name, const bool &Value)
{
    m_priv->SetSetting(Name, Value ? QString("1") : QString("0"));
}

void TorcLocalContext::SetSetting(const QString &Name, const int &Value)
{
    m_priv->SetSetting(Name, QString::number(Value));
}

void TorcLocalContext::SetUIObject(QObject *UI)
{
    if (m_priv->m_UIObject && UI)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Trying to register a second global UI - ignoring");
        return;
    }

    m_priv->m_UIObject = UI;
}

QObject* TorcLocalContext::GetUIObject(void)
{
    return m_priv->m_UIObject;
}

QLocale::Language TorcLocalContext::GetLanguage(void)
{
    return m_priv->m_language.GetLanguage();
}

void TorcLocalContext::CloseDatabaseConnections(void)
{
    if (m_priv && m_priv->m_sqliteDB)
        m_priv->m_sqliteDB->CloseThreadConnection();
}

bool TorcLocalContext::GetFlag(int Flag)
{
    return m_priv->m_flags & Flag;
}
