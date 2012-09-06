#ifndef TORCSTORAGEDEVICE_H
#define TORCSTORAGEDEVICE_H

// Qt
#include <QString>

// Torc
#include "torccoreexport.h"

class TORC_CORE_PUBLIC TorcStorageDevice
{
  public:
    enum StorageType
    {
        Unknown,
        FixedDisk,
        RemovableDisk,
        Optical,
        CD,
        DVD,
        BD,
        HDDVD
    };

    enum StorageProperties
    {
        None      = 0x0000,
        Busy      = 0x0001,
        Mounted   = 0x0002,
        Writeable = 0x0004,
        Ejectable = 0x0008,
        Removable = 0x0010
    };

    static QString TypeToString(StorageType Type);

  public:
    TorcStorageDevice();
    TorcStorageDevice(StorageType Type, int Properties, const QString &Name,
                      const QString &SystemName, const QString &Description);
    virtual ~TorcStorageDevice();

    StorageType GetType        (void);
    int         GetProperties  (void);
    QString     GetName        (void);
    QString     GetSystemName  (void);
    QString     GetDescription (void);
    bool        IsOpticalDisk  (void);

    void        SetType        (StorageType Type);
    void        SetProperties  (int Properties);
    void        SetName        (const QString &Name);
    void        SetSystemName  (const QString &SystemName);
    void        SetDescription (const QString &Description);

  protected:
    StorageType m_type;
    int         m_properties;
    QString     m_name;
    QString     m_systemName;
    QString     m_description;
};

#endif // TORCSTORAGEDEVICE_H
