#ifndef TORCSTORAGEOSX_H
#define TORCSTORAGEOSX_H

// OS X
#include <DiskArbitration/DiskArbitration.h>

// Torc
#include "torcstorage.h"

class TorcStorageOSX : public TorcStoragePriv
{
  public:
    TorcStorageOSX(TorcStorage *Parent);
   ~TorcStorageOSX();

    bool Mount   (const QString &Disk);
    bool Unmount (const QString &Disk);
    bool Eject   (const QString &Disk);

    static void               DiskAppearedCallback    (DADiskRef Disk, void *Context);
    static void               DiskDisappearedCallback (DADiskRef Disk, void *Context);
    static void               DiskChangedCallback     (DADiskRef Disk, CFArrayRef Keys, void *Context);
    static void               DiskMountCallback       (DADiskRef Disk, DADissenterRef Dissenter, void *Context);
    static void               DiskUnmountCallback     (DADiskRef Disk, DADissenterRef Dissenter, void *Context);
    static void               DiskEjectCallback       (DADiskRef Disk, DADissenterRef Dissenter, void *Context);
    static TorcStorageDevice  GetDiskDetails          (DADiskRef Disk);

  private:   
    void AddDisk       (DADiskRef Disk);
    void RemoveDisk    (DADiskRef Disk);
    void ChangeDisk    (DADiskRef Disk);
    void DiskMounted   (DADiskRef Disk);
    void DiskUnmounted (DADiskRef Disk);

  private:
    DASessionRef m_daSession;
};

#endif // TORCSTORAGEOSX_H
