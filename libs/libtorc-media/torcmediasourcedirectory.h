#ifndef TORCMEDIASOURCEDIRECTORY_H
#define TORCMEDIASOURCEDIRECTORY_H

// Qt
#include <QDir>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QFileSystemWatcher>
#include <QStringList>

// Torc
#include "torcmediaexport.h"
#include "http/torchttpservice.h"
#include "torcmedia.h"

class TorcMediaDirectory;

class TORC_MEDIA_PUBLIC TorcMediaSourceDirectory : public QObject, public TorcHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",            "1.0.0")
    Q_CLASSINFO("AddPath",            "methods=PUT")
    Q_CLASSINFO("RemovePath",         "methods=PUT")
    Q_CLASSINFO("GetConfiguredPaths", "type=paths")

  public:
    TorcMediaSourceDirectory();
    virtual ~TorcMediaSourceDirectory();

    Q_PROPERTY(QStringList configuredPaths READ GetConfiguredPaths NOTIFY configuredPathsChanged)
    Q_PROPERTY(int         mediaVersion    READ GetMediaVersion    NOTIFY mediaVersionChanged)

  public slots:
    void            SubscriberDeleted       (QObject *Subscriber);

    void            AddPath                 (const QString &Path, bool Recursive);
    void            RemovePath              (const QString &Path);
    QStringList     GetConfiguredPaths      (void);
    int             GetMediaVersion         (void);

  signals:
    void            mediaVersionChanged     (void);
    void            configuredPathsChanged  (void);

  protected slots:
    void            IncrementVersion        (void);
    void            DirectoryChanged        (const QString &Path);
    void            StartMonitoring         (void);
    void            StopMonitoring          (void);

  public:
    TorcMedia::MediaType GuessFileType      (const QString &Path);

  protected:
    void            customEvent             (QEvent *Event);
    void            timerEvent              (QTimerEvent *Event);

  private:
    void            RemovePaths             (QStringList &Paths);
    void            RemoveItem              (const QString &Path);
    void            AddDirectories          (const QString &Path, QStringList &Found);

  private:
    QFileSystemWatcher m_watcher;
    int             mediaVersion;
    QAtomicInt      realMediaVersion;
    QStringList     configuredPaths;

    bool            m_enabled;
    int             m_timerId;

    QDir::Filters   m_fileFilters;
    QDir::Filters   m_directoryFilters;
    QStringList     m_audioExtensions;
    QStringList     m_videoExtensions;
    QStringList     m_photoExtensions;
    QStringList     m_fileNameFilters;
    QStringList     m_directoryNameFilters;

    QHash<QString,TorcMediaDirectory*>   m_monitoredPaths;
    QHash<QString,TorcMediaDescription*> m_mediaItems;

    QStringList     m_addedPaths;
    QStringList     m_removedPaths;
    QStringList     m_updatedPaths;

    QMutex         *m_configuredPathsLock;
    QMutex         *m_addedPathsLock;
    QMutex         *m_removedPathsLock;
    QMutex         *m_updatedPathsLock;
};

#endif // TORCMEDIASOURCEDIRECTORY_H
