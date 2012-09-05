#ifndef TORCSTORAGEUNIXDBUS_H
#define TORCSTORAGEUNIXDBUS_H

// Qt
#include <QtDBus>

// Torc
#include "torcstorage.h"

class TorcStorageUnixDBus : public TorcStoragePriv
{
    Q_OBJECT

  public:
    TorcStorageUnixDBus(TorcStorage *Parent);
   ~TorcStorageUnixDBus();

    bool Mount   (const QString &Disk);
    bool Unmount (const QString &Disk);
    bool Eject   (const QString &Disk);

  public slots:
    void DiskAdded   (QDBusObjectPath Path);
    void DiskRemoved (QDBusObjectPath Path);
    void DiskChanged (QDBusObjectPath Path);

  private:
    TorcStorageDevice GetDiskDetails (const QString &Disk);

  private:
    QDBusInterface *m_udisksInterface;
};

#endif // TORCSTORAGEUNIXDBUS_H
