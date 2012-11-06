/* Class TorcStorageDevice
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012
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

TorcStorageDevice::StorageType TorcStorageDevice::GetType(void)
{
    return m_type;
}

int TorcStorageDevice::GetProperties(void)
{
    return m_properties;
}

QString TorcStorageDevice::GetName(void)
{
    return m_name;
}

QString TorcStorageDevice::GetSystemName(void)
{
    return m_systemName;
}

QString TorcStorageDevice::GetDescription(void)
{
    return m_description;
}

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
