/* Class TorcMediaSourceDirectory
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2013
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

// Qt
#include <QObject>
#include <QDirIterator>
#include <QCoreApplication>

// Torc
#include "torclocalcontext.h"
#include "torclogging.h"
#include "torcmime.h"
#include "torcmediamaster.h"
#include "torcmediasource.h"
#include "torcmediasourcedirectory.h"

// process updates every 5 seconds
#define UPDATE_FREQUENCY  5000
#define RECURSIVE_PATH    QString("rECuRSiVE")
#define LOCAL_DIRECTORIES (TORC_CORE + QString("LocalMediaDirectories"))

/*! \class TorcMediaSourceDirectory
 *  \brief A class to monitor file systems for media content.
 *
 * TorcMediaSourceDirectory uses QFileSystemWatcher to monitor local file systems.
 * QFileSystemWatcher will only notify updates for individual directories - and not
 * subdirectories. So if we want to monitor sub-directories as well (i.e. recursively),
 * we need to explicitly watch each individual sub-directory.
 *
 * m_configuredPaths contains a definitive set of paths that have been requested
 * by the user and should match the settings database. Updates should be made
 * using AddPath and RemovePath.
 *
 * m_monitoredPaths contains a complete list of all paths currently being watched,
 * including all sub-directories of paths requested to be monitored recursively.
 *
 * For performance reasons, identified files are currently not interrogated in
 * any way (e.g. actual content, file size, creation date, modification date etc).
 *
 * \sa MediaSource
 *
 * \todo Actually use the identified media files.
 * \todo Directories retrieved from settings are all assumed to be recursive.
 * \todo Configurable media extensions.
*/

class TorcMediaDirectory
{
  public:
    TorcMediaDirectory(bool Recursive = false)
      : m_recursive(Recursive)
    {
    }

    bool        m_recursive;
    QStringList m_knownFiles;
    QStringList m_knownDirectories;
};

TorcMediaSourceDirectory::TorcMediaSourceDirectory()
  : QFileSystemWatcher(),
    m_enabled(false),
    m_timerId(0),
    m_fileFilters(QDir::NoDotAndDotDot | QDir::Files | QDir::Readable),
    m_directoryFilters(QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Readable),
    m_addedPathsLock(new QMutex()),
    m_removedPathsLock(new QMutex()),
    m_updatedPathsLock(new QMutex())
{
    // name filters
    m_videoExtensions = TorcMime::ExtensionsForType("video");
    m_audioExtensions = TorcMime::ExtensionsForType("audio");
    m_photoExtensions = TorcMime::ExtensionsForType("image");

    foreach (QString extension, m_videoExtensions) { m_fileNameFilters << QString("*." + extension); }
    foreach (QString extension, m_audioExtensions) { m_fileNameFilters << QString("*." + extension); }
    foreach (QString extension, m_photoExtensions) { m_fileNameFilters << QString("*." + extension); }

    LOG(VB_GENERAL, LOG_INFO, QString("Video extensions: %1").arg(m_videoExtensions.join(",")));
    LOG(VB_GENERAL, LOG_INFO, QString("Audio extensions: %1").arg(m_audioExtensions.join(",")));
    LOG(VB_GENERAL, LOG_INFO, QString("Image extensions: %1").arg(m_photoExtensions.join(",")));

    // configured directories to monitor
    QString directories = gLocalContext->GetSetting(LOCAL_DIRECTORIES, QString(""));
    foreach (QString path, directories.split(","))
    {
        path = path.trimmed();
        if (!path.isEmpty())
            m_configuredPaths << path;
    }

    //m_configuredPaths << "/Users/mark/Dropbox/";

    // connect up the dots
    connect(this, SIGNAL(directoryChanged(QString)), this, SLOT(DirectoryChanged(QString)));
    gLocalContext->AddObserver(this);

    // start
    if (gLocalContext->GetSetting(TORC_CORE + "StorageEnabled", (bool)true))
        StartMonitoring();

    m_timerId = startTimer(UPDATE_FREQUENCY);
}

TorcMediaSourceDirectory::~TorcMediaSourceDirectory()
{
    if (m_timerId)
        killTimer(m_timerId);

    gLocalContext->RemoveObserver(this);

    StopMonitoring();

    delete m_addedPathsLock;
    delete m_removedPathsLock;
    delete m_updatedPathsLock;
}

void TorcMediaSourceDirectory::AddPath(const QString &Path, bool Recursive)
{
    QMutexLocker locker(m_addedPathsLock);
    m_addedPaths << (Recursive ? RECURSIVE_PATH : QString("")) + Path.trimmed();
}

void TorcMediaSourceDirectory::RemovePath(const QString &Path)
{
    QMutexLocker locker(m_removedPathsLock);
    m_removedPaths << Path.trimmed();
}

void TorcMediaSourceDirectory::DirectoryChanged(const QString &Path)
{
    LOG(VB_GENERAL, LOG_INFO, Path);

    m_updatedPathsLock->lock();
    if (!m_updatedPaths.contains(Path))
        m_updatedPaths << Path;
    m_updatedPathsLock->unlock();
}

TorcMedia::MediaType TorcMediaSourceDirectory::GuessFileType(const QString &Path)
{
    QStringList list = Path.split(".");
    if (!list.isEmpty())
    {
        QString extension = list.last().toLower();

        if (m_audioExtensions.contains(extension))
            return TorcMedia::MediaTypeMusic;

        if (m_photoExtensions.contains(extension))
            return TorcMedia::MediaTypePicture;

        if (m_videoExtensions.contains(extension))
            return TorcMedia::MediaTypeMovie;
    }

    return TorcMedia::MediaTypeNone;
}

void TorcMediaSourceDirectory::customEvent(QEvent *Event)
{
    if (Event->type() == TorcEvent::TorcEventType)
    {
        TorcEvent* torcevent = dynamic_cast<TorcEvent*>(Event);
        if (torcevent)
        {
            int event = torcevent->GetEvent();

            if (event == Torc::DisableStorage)
            {
                StopMonitoring();
                return;
            }
            else if (event == Torc::EnableStorage)
            {
                StartMonitoring();
                return;
            }
        }
    }
}

void TorcMediaSourceDirectory::timerEvent(QTimerEvent *Event)
{
    if (Event->timerId() != m_timerId)
        return;

    // remove paths removed from configuration
    m_removedPathsLock->lock();
    QStringList removedpaths = m_removedPaths;
    m_removedPaths.clear();
    m_removedPathsLock->unlock();

    RemovePaths(removedpaths);

    // add newly configured paths
    QStringList updatedpaths;

    m_addedPathsLock->lock();
    QStringList addedpaths = m_addedPaths;
    m_addedPaths.clear();
    m_addedPathsLock->unlock();

    QStringList addtoconfigured;

    foreach (QString path, addedpaths)
    {
        bool recursive = false;
        if (path.startsWith(RECURSIVE_PATH))
        {
            recursive = true;
            path.replace(RECURSIVE_PATH, "");
        }

        if (!m_configuredPaths.contains(path))
            addtoconfigured << path;

        if (m_enabled && !m_monitoredPaths.contains(path))
        {
            m_monitoredPaths.insert(path, new TorcMediaDirectory(recursive));

            updatedpaths << path; // force a scan
            addPath(path);
            LOG(VB_GENERAL, LOG_INFO, QString("Started monitoring '%1' (Recursive: %2)").arg(path).arg(recursive));
        }
    }

    // update settings
    bool changed = false;
    foreach (QString path, removedpaths)
        if (m_configuredPaths.removeAll(path))
            changed = true;

    foreach (QString path, addtoconfigured)
    {
        m_configuredPaths << path;
        changed = true;
    }

    if (changed)
    {
        m_configuredPaths.removeDuplicates();
        gLocalContext->SetSetting(LOCAL_DIRECTORIES, m_configuredPaths.join(","));
    }

    // refresh existing paths (and newly added)
    m_updatedPathsLock->lock();
    updatedpaths << m_updatedPaths;
    m_updatedPaths.clear();
    m_updatedPathsLock->unlock();

    if (m_enabled)
    {
        // check for new directories first (if recursive)
        foreach (QString path, updatedpaths)
        {
            if (!m_monitoredPaths.contains(path))
                continue;

            TorcMediaDirectory *directory = m_monitoredPaths.value(path);
            if (!directory->m_recursive)
                continue;

            QStringList newdirectories;
            QStringList olddirectories = directory->m_knownDirectories;
            directory->m_knownDirectories.clear();

            QDirIterator it(path, m_directoryNameFilters, m_directoryFilters);
            while (it.hasNext())
            {
                it.next();
                QString dir = it.filePath();
                directory->m_knownDirectories << dir;
                if (olddirectories.contains(dir))
                    olddirectories.removeAll(dir);
                else
                    newdirectories << dir;
            }

            // add new directories
            foreach (QString newdirectory, newdirectories)
            {
                LOG(VB_GENERAL, LOG_INFO, QString("Added directory '%1'").arg(newdirectory));
                m_monitoredPaths.insert(newdirectory, new TorcMediaDirectory(true));
                addPath(newdirectory);
            }

            // new directories will be processed on the next call, which simplifies
            // the code and adds some inherent rate limiting.
            m_updatedPathsLock->lock();
            m_updatedPaths << newdirectories;
            m_updatedPathsLock->unlock();

            // and remove old
            RemovePaths(olddirectories);
        }

        // check for new files
        QVariantList notifynew;
        QVariantList notifyold;

        foreach (QString path, updatedpaths)
        {
            if (!m_monitoredPaths.contains(path))
                continue;

            TorcMediaDirectory *directory = m_monitoredPaths.value(path);
            QStringList oldfiles = directory->m_knownFiles;
            directory->m_knownFiles.clear();

            QDirIterator it(path, m_fileNameFilters, m_fileFilters);
            while (it.hasNext())
            {
                it.next();
                QString name = it.filePath();
                if (!m_mediaItems.contains(name))
                {
                    TorcMedia::MediaType type = GuessFileType(name);
                    TorcMediaDescription *media = new TorcMediaDescription(it.fileName(), name, type,
                                                                TorcMedia::MediaSourceLocal, NULL);
                    m_mediaItems.insert(name, media);
                    notifynew.append(QVariant::fromValue(*media));

                    LOG(VB_GENERAL, LOG_INFO, QString("Added '%1'").arg(name));
                }

                directory->m_knownFiles << name;
                oldfiles.removeAll(name);
            }

            // remove old
            foreach (QString name, oldfiles)
            {
                LOG(VB_GENERAL, LOG_INFO, QString("File '%1' no longer available").arg(name));
                TorcMediaDescription *media = m_mediaItems.take(name);
                notifyold.append(QVariant::fromValue(*media));
                delete media;
            }
        }

        if (!notifynew.isEmpty() && gTorcMediaMaster)
        {
            QVariantMap data;
            data.insert("files", notifynew);
            TorcEvent *event = new TorcEvent(Torc::MediaAdded, data);
            QCoreApplication::postEvent(gTorcMediaMaster, event);
        }

        if (!notifyold.isEmpty() && gTorcMediaMaster)
        {
            QVariantMap data;
            data.insert("files", notifyold);
            TorcEvent *event = new TorcEvent(Torc::MediaRemoved, data);
            QCoreApplication::postEvent(gTorcMediaMaster, event);
        }
    }
}

void TorcMediaSourceDirectory::StartMonitoring(void)
{
    if (m_enabled)
        return;

    m_enabled = true;

    // start monitoring each configured directory
    foreach (QString path, m_configuredPaths)
    {
        addPath(path);
        m_monitoredPaths.insert(path, new TorcMediaDirectory(true));
        LOG(VB_GENERAL, LOG_INFO, QString("Starting to monitor '%1' (Recursive: 1)").arg(path));
    }

    // force refresh
    m_updatedPathsLock->lock();
    m_updatedPaths << m_configuredPaths;
    m_updatedPathsLock->unlock();
}

void TorcMediaSourceDirectory::StopMonitoring(void)
{
    if (!m_enabled)
        return;

    m_enabled = false;

    // stop monitoring everything
    QStringList paths;
    QHash<QString,TorcMediaDirectory*>::iterator it = m_monitoredPaths.begin();
    for ( ; it != m_monitoredPaths.end(); ++it)
        paths << it.key();

    RemovePaths(paths);

    // check everthing is cleared
    if (!directories().isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Qt is still monitoring %1 directories - stopping").arg(directories().size()));
        removePaths(directories());
    }

    if (!m_mediaItems.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Still tracking %1 media items - removing").arg(m_mediaItems.size()));

        QHash<QString,TorcMediaDescription*>::iterator it = m_mediaItems.begin();
        for ( ; it != m_mediaItems.end(); ++it)
        {
            delete it.value();
            LOG(VB_GENERAL, LOG_INFO, QString("Removed '%1'").arg(it.key()));
        }
    }
}

void TorcMediaSourceDirectory::RemovePaths(QStringList &Paths)
{
    QVariantList notifyold;

    foreach (QString path, Paths)
    {
        if (!m_monitoredPaths.contains(path))
            continue;

        TorcMediaDirectory *directory = m_monitoredPaths.value(path);

        // remove all directories if recursively monitored
        if (directory->m_recursive)
        {
            QStringList paths;
            QDirIterator it(path, m_directoryNameFilters, m_directoryFilters);
            while (it.hasNext())
            {
                it.next();
                paths << it.filePath();
            }

            RemovePaths(paths);
        }

        // cleanup
        QStringList knownpaths = directory->m_knownFiles;
        delete directory;
        m_monitoredPaths.remove(path);
        removePath(path);

        LOG(VB_GENERAL, LOG_INFO, QString("Stopping monitoring '%1'").arg(path));

        // remove items
        QDirIterator it(path, m_fileNameFilters, m_fileFilters);
        while (it.hasNext())
        {
            it.next();
            knownpaths.removeOne(it.filePath());
            TorcMediaDescription *media = m_mediaItems.take(it.filePath());
            notifyold.append(QVariant::fromValue(*media));
            delete media;
        }

        foreach (QString leftover, knownpaths)
            RemoveItem(leftover);
    }

    if (!notifyold.isEmpty() && gTorcMediaMaster)
    {
        QVariantMap data;
        data.insert("files", notifyold);
        TorcEvent *event = new TorcEvent(Torc::MediaRemoved, data);
        QCoreApplication::postEvent(gTorcMediaMaster, event);
    }
}

void TorcMediaSourceDirectory::RemoveItem(const QString &Path)
{
    if (m_mediaItems.contains(Path))
    {
        delete m_mediaItems.take(Path);
        LOG(VB_GENERAL, LOG_INFO, QString("Removed '%1'").arg(Path));
    }
}

class TorcDirectoryMonitor : public TorcMediaSource
{
  public:
    TorcDirectoryMonitor()
     :  TorcMediaSource(),
        m_directoryMonitor(NULL)
    {
    }

    void Create(void)
    {
        if (gLocalContext->FlagIsSet(Torc::Storage))
            m_directoryMonitor = new TorcMediaSourceDirectory();
    }

    void Destroy(void)
    {
        delete m_directoryMonitor;
    }

  private:
    TorcMediaSourceDirectory *m_directoryMonitor;

} TorcDirectoryMonitor;
