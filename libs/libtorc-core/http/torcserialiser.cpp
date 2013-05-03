/* Class TorcSerialiser
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

// Torc
#include "torcserialiser.h"

TorcSerialiser::TorcSerialiser()
  : m_content(new QByteArray())
{
}

TorcSerialiser::~TorcSerialiser()
{
    delete m_content;
}

QByteArray* TorcSerialiser::Serialise(const QVariant &Data)
{
    Prepare();
    Begin();
    AddProperty(Data.typeName(), Data);
    End();

    // pass ownership of content to caller
    QByteArray *result = m_content;
    m_content = NULL;
    return result;
}
