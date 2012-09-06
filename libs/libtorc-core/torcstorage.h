#ifndef TORCSTORAGE_H
#define TORCSTORAGE_H

// Qt
#include <QObject>
#include <QMap>

// Torc
#include "torccoreexport.h"

class QMutex;
class TorcStorageDevice;
class TorcStorage;

class TorcStoragePriv : public QObject
{
    Q_OBJECT

  public:
    TorcStoragePriv(TorcStorage *Parent);
    virtual ~TorcStoragePriv();

  public:
    virtual bool Mount       (const QString &Disk) = 0;
    virtual bool Unmount     (const QString &Disk) = 0;
    virtual bool Eject       (const QString &Disk) = 0;
    virtual bool ReallyEject (const QString &Disk) = 0;
};

class TORC_CORE_PUBLIC TorcStorage : public QObject
{
    Q_OBJECT

  public:
    static void Create        (void);
    static void Destroy       (void);
    static bool DiskIsMounted (const QString &Disk);

  public:
    void AddDisk       (TorcStorageDevice &Disk);
    void RemoveDisk    (TorcStorageDevice &Disk);
    void ChangeDisk    (TorcStorageDevice &Disk);
    void DiskMounted   (TorcStorageDevice &Disk);
    void DiskUnmounted (TorcStorageDevice &Disk);

    bool Mount         (const QString     &Disk);
    bool Unmount       (const QString     &Disk);
    bool Eject         (const QString     &Disk);

  protected:
    TorcStorage();
    virtual ~TorcStorage();

  protected:
    QMap<QString,TorcStorageDevice> m_disks;
    QMutex                         *m_disksLock;
    TorcStoragePriv                *m_priv;
};

extern TORC_CORE_PUBLIC TorcStorage *gStorage;
extern TORC_CORE_PUBLIC QMutex      *gStorageLock;
#endif // TORCSTORAGE_H
