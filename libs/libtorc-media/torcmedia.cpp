/* Class TorcMedia
*
* This file is part of the Torc project.
*
* Copyright (C) Robert McNamara 2012
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
#include "torcmedia.h"
#include "torcmetadata.h"

/** \class TorcMedia
 *  \brief Class representing a piece of consumable media (video, music, image)
 *
 *  Most fundamental element of a piece of media, with a textual name and a
 *  path/URI.  One can assign an optional pointer to a TorcMetadata object
 *  which is a context-appropriate object with title, subtitle, starttime,
 *  artist, etc. for use in the UI, the API, and elsewhere.
 */

TorcMedia::TorcMedia(const QString Name,
                     const QString URL,
                     TorcMediaType Type,
                     TorcMetadata *Metadata)
  : m_valid(true),
    m_name(Name),
    m_url(URL),
    m_type(Type),
    m_metadata(Metadata)
{
}

TorcMedia::~TorcMedia()
{
    if (m_metadata)
        delete m_metadata;
}

bool TorcMedia::IsValid(void)
{
    return m_valid;
}

void TorcMedia::Invalidate(void)
{
    m_valid = false;
}

QString TorcMedia::GetName(void)
{
    return m_name;
}

QString TorcMedia::GetURL(void)
{
    return m_url;
}

TorcMetadata* TorcMedia::GetMetadata(void)
{
    return m_metadata;
}

void TorcMedia::SetName(const QString &Name)
{
    m_name = Name;
}

void TorcMedia::SetURL(const QString &URL)
{
    m_url = URL;
}

void TorcMedia::SetMetadata(TorcMetadata *Metadata)
{
    m_metadata = Metadata;
}

TorcMediaType TorcMedia::GetMediaType(void)
{
    return m_type;
}

void TorcMedia::SetMediaType(TorcMediaType Type)
{
    m_type = Type;
}
