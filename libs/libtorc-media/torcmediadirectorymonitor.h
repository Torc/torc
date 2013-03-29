#ifndef TORCMEDIADIRECTORYMONITOR_H
#define TORCMEDIADIRECTORYMONITOR_H

// Qt
#include <QObject>
#include <QFileSystemWatcher>
#include <QStringList>

// Torc
#include "torcmediaexport.h"

class TORC_MEDIA_PUBLIC TorcMediaDirectoryMonitor : public QFileSystemWatcher
{
    Q_OBJECT

  public:
    TorcMediaDirectoryMonitor();
    virtual ~TorcMediaDirectoryMonitor();

  public slots:
    void       DirectoryChanged   (const QString &Path);
};

#endif // TORCMEDIADIRECTORYMONITOR_H
