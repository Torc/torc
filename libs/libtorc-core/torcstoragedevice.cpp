/* Class TorcStorageDevice
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
#include <QObject>

// Torc
#include "torcstoragedevice.h"

/*! \class TorcStorageDevice
 *  \brief A platform independent description of a storage device (hard disk drive, optical drive etc)
*/
TorcStorageDevice::TorcStorageDevice(StorageType Type, int Properties,
                                     const QString &Name,
                                     const QString &SystemName,
                                     const QString &Description)
  : m_type(Type),
    m_properties(Properties),
    m_name(Name),
    m_systemName(SystemName),
    m_description(Description)
{
}

TorcStorageDevice::TorcStorageDevice()
  : m_type(Unknown),
    m_properties(None)
{
}

TorcStorageDevice::~TorcStorageDevice()
{
}

///brief Return a human readable string for the given type.
QString TorcStorageDevice::TypeToString(StorageType Type)
{
    switch (Type)
    {
        case FixedDisk:     return QObject::tr("Fixed Disk");
        case RemovableDisk: return QObject::tr("Removable Disk");
        case Optical:       return QObject::tr("Optical Disk");
        case DVD:           return QObject::tr("DVD");
        case CD:            return QObject::tr("CD");
        case BD:            return QObject::tr("BD");
        case HDDVD:         return QObject::tr("HDDVD");
        default:            break;
    }

    return QString("Unknown");
}

///brief Create a QVariantMap that describes this device.
QVariantMap TorcStorageDevice::ToMap(void)
{
    QVariantMap result;
    result.insert("name",        m_name);
    result.insert("systemname",  m_systemName);
    result.insert("description", m_description);
    result.insert("optical",     IsOpticalDisk());
    result.insert("type",        TypeToString(m_type));
    result.insert("busy",        (bool)(m_properties & Busy));
    result.insert("mounted",     (bool)(m_properties & Mounted));
    result.insert("writeable",   (bool)(m_properties & Writeable));
    result.insert("ejectable",   (bool)(m_properties & Ejectable));
    result.insert("removable",   (bool)(m_properties & Removable));
    return result;
}

///brief Return the type of this storage device.
TorcStorageDevice::StorageType TorcStorageDevice::GetType(void)
{
    return m_type;
}

///brief Return the properties for this device (ejectable, mountable etc)
int TorcStorageDevice::GetProperties(void)
{
    return m_properties;
}

///brief Return the user friendly name for this device.
QString TorcStorageDevice::GetName(void)
{
    return m_name;
}

///brief Return the platform specfic identifier for this device.
QString TorcStorageDevice::GetSystemName(void)
{
    return m_systemName;
}

///brief Return the device description.
QString TorcStorageDevice::GetDescription(void)
{
    return m_description;
}

///brief Return true if this device is an optical drive, false otherwise.
bool TorcStorageDevice::IsOpticalDisk(void)
{
    return m_type == CD || m_type == BD || m_type == DVD || m_type == HDDVD || m_type == Optical;
}

void TorcStorageDevice::SetType(StorageType Type)
{
    m_type = Type;

    // this may need revisiting
    if (IsOpticalDisk())
        SetProperties(GetProperties() & ~Writeable);
}

void TorcStorageDevice::SetProperties(int Properties)
{
    m_properties = Properties;
}

void TorcStorageDevice::SetName(const QString &Name)
{
    m_name = Name;
}

void TorcStorageDevice::SetSystemName(const QString &SystemName)
{
    m_systemName = SystemName;
}

void TorcStorageDevice::SetDescription(const QString &Description)
{
    m_description = Description;
}
