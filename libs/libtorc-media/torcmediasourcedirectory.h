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
#include "torcmedia.h"

class TorcMediaDirectory;

class TORC_MEDIA_PUBLIC TorcMediaSourceDirectory : public QFileSystemWatcher
{
    Q_OBJECT

  public:
    TorcMediaSourceDirectory();
    virtual ~TorcMediaSourceDirectory();

  public slots:
    void            AddPath            (const QString &Path, bool Recursive);
    void            RemovePath         (const QString &Path);

  protected slots:
    void            DirectoryChanged   (const QString &Path);
    void            StartMonitoring    (void);
    void            StopMonitoring     (void);

  public:
    TorcMediaType   GuessFileType      (const QString &Path);

  protected:
    void            customEvent        (QEvent *Event);
    void            timerEvent         (QTimerEvent *Event);

  private:
    void            RemovePaths        (QStringList &Paths);
    void            RemoveItem         (const QString &Path);
    void            AddDirectories     (const QString &Path, QStringList &Found);

  private:
    QStringList     m_configuredPaths;

    bool            m_enabled;
    int             m_timerId;

    QDir::Filters   m_fileFilters;
    QDir::Filters   m_directoryFilters;
    QStringList     m_audioExtensions;
    QStringList     m_videoExtensions;
    QStringList     m_photoExtensions;
    QStringList     m_fileNameFilters;
    QStringList     m_directoryNameFilters;

    QHash<QString,TorcMediaDirectory*> m_monitoredPaths;
    QHash<QString,TorcMedia*>          m_mediaItems;

    QStringList     m_addedPaths;
    QStringList     m_removedPaths;
    QStringList     m_updatedPaths;

    QMutex         *m_addedPathsLock;
    QMutex         *m_removedPathsLock;
    QMutex         *m_updatedPathsLock;
};

#endif // TORCMEDIASOURCEDIRECTORY_H
