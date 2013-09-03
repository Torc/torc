/* Class TorcStorage
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012-13
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
#include <QMutex>
#include <QDir>

// Torc
#include "torccoreutils.h"
#include "torclocalcontext.h"
#include "torclogging.h"
#include "torcadminthread.h"
#include "torchttpserver.h"
#include "torcstoragedevice.h"
#include "torcstorage.h"

#if defined(Q_OS_MAC)
#include "torcstorageosx.h"
#elif CONFIG_QTDBUS
#include "torcstorageunixdbus.h"
#endif

TorcStorage* gStorage     = NULL;
QMutex*      gStorageLock = new QMutex(QMutex::Recursive);

TorcStoragePriv::TorcStoragePriv(TorcStorage *Parent)
  : QObject(static_cast<QObject*>(Parent))
{
}

TorcStoragePriv::~TorcStoragePriv()
{
}

void TorcStorage::Create(void)
{
    QMutexLocker locker(gStorageLock);

    if (gStorage)
        return;

    gStorage = new TorcStorage();
}

void TorcStorage::Destroy(void)
{
    QMutexLocker locker(gStorageLock);

    if (gStorage)
        gStorage->deleteLater();
    gStorage = NULL;
}

bool TorcStorage::DiskIsMounted(const QString &Disk)
{
    if (Disk.isEmpty())
        return false;

    QDir dir(Disk);

    int tries = 0;
    int wait = 50000;
    if (!dir.exists() && --tries < 30)
        QThread::usleep(wait += 10000);

    if (dir.exists())
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("Disk '%1' confirmed").arg(Disk));
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, QString("Failed to confirm disk '%1'").arg(Disk));
    return false;
}

TorcStorage::TorcStorage()
  : QObject(),
    TorcHTTPService(this, "storage", tr("Storage"), TorcStorage::staticMetaObject),
    m_disksLock(new QMutex(QMutex::Recursive)),
    m_priv(NULL)
{
#if defined(Q_OS_MAC)
    m_priv = new TorcStorageOSX(this);
#elif CONFIG_QTDBUS
    m_priv = new TorcStorageUnixDBus(this);
#endif
}

TorcStorage::~TorcStorage()
{
    // delete the implementation
    if (m_priv)
        m_priv->deleteLater();
    m_priv = NULL;

    // delete the lock
    delete m_disksLock;
    m_disksLock = NULL;
}

QVariantMap TorcStorage::GetDisks(void)
{
    QMutexLocker locker(m_disksLock);

    QVariantMap result;
    QMap<QString,TorcStorageDevice>::iterator it = m_disks.begin();
    for ( ; it != m_disks.end(); ++it)
        result.insert(it.key(), it.value().ToMap());

    return result;
}

void TorcStorage::AddDisk(TorcStorageDevice &Disk)
{
    if (!Disk.GetSystemName().isEmpty() && (Disk.GetType() != TorcStorageDevice::Unknown))
    {
        QMutexLocker locker(m_disksLock);

        if (m_disks.contains(Disk.GetSystemName()))
        {
            LOG(VB_GENERAL, LOG_DEBUG, QString("Disk '%1' already discovered").arg(Disk.GetName()));

            if (m_disks[Disk.GetSystemName()].GetName() != Disk.GetName())
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("Disk '%1' already discovered with a different volume name (old %2 new %3)")
                    .arg(Disk.GetSystemName())
                    .arg(m_disks[Disk.GetSystemName()].GetName())
                    .arg(Disk.GetName()));
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, QString("New %1 '%2' at '%3'")
                .arg(TorcStorageDevice::TypeToString(Disk.GetType()))
                .arg(Disk.GetSystemName())
                .arg((Disk.GetProperties() & TorcStorageDevice::Mounted) ? Disk.GetName() : "Unmounted"));

            // confirm the disk is mounted if it is reported as such
            if ((Disk.GetProperties() & TorcStorageDevice::Mounted) && !DiskIsMounted(Disk.GetName()))
            {
                Disk.SetProperties(Disk.GetProperties() & ~TorcStorageDevice::Mounted);
            }

            LOG(VB_GENERAL, LOG_DEBUG, Disk.GetDescription());

            int p = Disk.GetProperties();
            LOG(VB_GENERAL, LOG_DEBUG, QString("Writeable %1 Removable %2 Ejectable %3 Mounted %4")
                .arg((bool)(p & TorcStorageDevice::Writeable))
                .arg((bool)(p & TorcStorageDevice::Removable))
                .arg((bool)(p & TorcStorageDevice::Ejectable))
                .arg((bool)(p & TorcStorageDevice::Mounted)));

            m_disks.insert(Disk.GetSystemName(), Disk);
        }
    }
}

void TorcStorage::RemoveDisk(TorcStorageDevice &Disk)
{
    if (!Disk.GetSystemName().isEmpty())
    {
        QMutexLocker locker(m_disksLock);

        if (m_disks.contains(Disk.GetSystemName()))
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Disk %1 (%2) went away")
                .arg(m_disks[Disk.GetSystemName()].GetName())
                .arg(Disk.GetSystemName()));
            m_disks.remove(Disk.GetSystemName());
        }
    }
}

void TorcStorage::ChangeDisk(TorcStorageDevice &Disk)
{
    QString name = Disk.GetSystemName();
    if (name.isEmpty())
        return;

    QMutexLocker locker(m_disksLock);

    QMap<QString,TorcStorageDevice>::iterator it = m_disks.find(name);

    if (it == m_disks.end())
    {
        AddDisk(Disk);
        return;
    }

    if ((*it).GetName() != Disk.GetName())
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Disk '%1' changed name from '%2' to '%3'")
            .arg(name).arg((*it).GetName()).arg(Disk.GetName()));
        (*it).SetName(Disk.GetName());
    }

    if ((*it).GetType() != Disk.GetType())
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Disk '%1' changed type from '%2' to '%3'")
            .arg(name)
            .arg(TorcStorageDevice::TypeToString((*it).GetType()))
            .arg(TorcStorageDevice::TypeToString(Disk.GetType())));
        (*it).SetType(Disk.GetType());
    }

    bool wasmounted = (*it).GetProperties() & TorcStorageDevice::Mounted;
    bool ismounted  = Disk.GetProperties() & TorcStorageDevice::Mounted;

    if (ismounted && !wasmounted)
        DiskMounted(Disk);
    else if (wasmounted && !ismounted)
        DiskUnmounted(Disk);
}

void TorcStorage::DiskMounted(TorcStorageDevice &Disk)
{
    if (!Disk.GetSystemName().isEmpty())
    {
        QMutexLocker locker(m_disksLock);

        if (m_disks.contains(Disk.GetSystemName()))
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Disk '%1' mounted at '%2'")
                .arg(Disk.GetSystemName())
                .arg(m_disks[Disk.GetSystemName()].GetName()));

            int properties = m_disks[Disk.GetSystemName()].GetProperties();
            properties = properties | TorcStorageDevice::Mounted;
            m_disks[Disk.GetSystemName()].SetProperties(properties);
        }
    }
}

void TorcStorage::DiskUnmounted(TorcStorageDevice &Disk)
{
    if (!Disk.GetSystemName().isEmpty())
    {
        QMutexLocker locker(m_disksLock);

        if (m_disks.contains(Disk.GetSystemName()))
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Disk '%1' unmounted")
                .arg(Disk.GetSystemName()))
                    ;
            int properties = m_disks[Disk.GetSystemName()].GetProperties();
            properties = properties & ~TorcStorageDevice::Mounted;
            m_disks[Disk.GetSystemName()].SetProperties(properties);

            if (Disk.GetType() == TorcStorageDevice::RemovableDisk)
            {
                // FIXME on OS X the disk doesn't actually get removed via RemoveDisk
                // so we still have a record of it but it is unmounted. It is
                // ejected from finder and ejecting directly from finder triggers
                // the RemoveDisk callback correctly...
                LOG(VB_GENERAL, LOG_INFO, "Disk unmounted and can be safely removed");
                TorcLocalContext::NotifyEvent(Torc::DiskCanBeSafelyRemoved);
            }
            else if (Disk.IsOpticalDisk())
            {
                m_priv->ReallyEject(Disk.GetSystemName());
            }
        }
    }
}

bool TorcStorage::Mount(const QString &Disk)
{
    // NB this doesn't check if the disk is already mounted, just in
    // case we've lost track somewhere
    {
        QMutexLocker locker(m_disksLock);

        if (!m_disks.contains(Disk))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Cannot mount '%1' - unknown")
                .arg(Disk));
            return false;
        }
    }

    if (m_priv)
        return m_priv->Mount(Disk);

    return false;
}

bool TorcStorage::Unmount(const QString &Disk)
{
    {
        QMutexLocker locker(m_disksLock);

        if (!m_disks.contains(Disk))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Cannot unmount '%1' - unknown")
                .arg(Disk));
            return false;
        }
    }

    if (m_priv)
        return m_priv->Unmount(Disk);

    return false;
}

bool TorcStorage::Eject(const QString &Disk)
{
    {
        QMutexLocker locker(m_disksLock);

        if (!m_disks.contains(Disk))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Cannot eject '%1' - unknown")
                .arg(Disk));
            return false;
        }
        else
        {
            if (!(m_disks[Disk].GetProperties() & TorcStorageDevice::Ejectable))
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Cannot eject '%1' - not ejectable")
                    .arg(Disk));
                return false;
            }
        }
    }

    if (m_priv)
        return m_priv->Eject(Disk);

    return false;
}

static class TorcStorageObject : public TorcAdminObject
{
  public:
    TorcStorageObject()
      : TorcAdminObject(TORC_ADMIN_MED_PRIORITY)
    {
    }

    void Create(void)
    {
        if (gLocalContext->FlagIsSet(Torc::Storage))
            TorcStorage::Create();
    }

    void Destroy(void)
    {
        TorcStorage::Destroy();
    }

} TorcStorageObject;
