/* Class TorcUPNP
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

// Torc
#include "torcupnp.h"

TorcUPNPDescription::TorcUPNPDescription()
  : m_usn(QString()),
    m_type(QString()),
    m_location(QString()),
    m_expiry(-1)
{
}

TorcUPNPDescription::TorcUPNPDescription(const QString &USN, const QString &Type, const QString &Location, qint64 Expires)
  : m_usn(USN),
    m_type(Type),
    m_location(Location),
    m_expiry(Expires)
{
}

QString TorcUPNPDescription::GetUSN(void) const
{
    return m_usn;
}

QString TorcUPNPDescription::GetType(void) const
{
    return m_type;
}

QString TorcUPNPDescription::GetLocation(void) const
{
    return m_location;
}

qint64 TorcUPNPDescription::GetExpiry(void) const
{
    return m_expiry;
}

void TorcUPNPDescription::SetExpiry(qint64 Expires)
{
    m_expiry = Expires;
}

QString TorcUPNP::UUIDFromUSN(const QString &USN)
{
    QString result;
    QString usn = USN.trimmed();

    if (usn.startsWith("uuid:"))
    {
        usn = usn.mid(5);
        int index = usn.indexOf("::");

        if (index > -1)
            result = usn.left(index);
    }

    return result;
}

