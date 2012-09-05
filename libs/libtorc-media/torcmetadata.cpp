/* Class TorcMetadata
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
#include "torcmetadata.h"
#include "torcmedia.h"

/** \class TorcMetadata
 *  \brief Class representing display information for a piece of media
 *
 *  Title, subtitle, and other sorts of relevant information to display
 *  or otherwise use for UI purposes.
 */

TorcMetadata::TorcMetadata(TorcMedia*    Media,
                           const QString Title,
                           const QString InternetID,
                           int           CollectionID)
  : m_media(Media),
    m_title(Title),
    m_internetID(InternetID),
    m_collectionID(CollectionID)
{
}

TorcMetadata::~TorcMetadata()
{
}

TorcMedia* TorcMetadata::GetMedia(void)
{
    return m_media;
}

void TorcMetadata::SetMedia(TorcMedia* Media)
{
    m_media = Media;
}

QString TorcMetadata::GetTitle(void)
{
    return m_title;
}

void TorcMetadata::SetTitle(const QString &Title)
{
    m_title = Title;
}

QString TorcMetadata::GetInternetID(void)
{
    return m_internetID;
}

void TorcMetadata::SetInternetID(const QString &InternetID)
{
    m_internetID = InternetID;
}

int TorcMetadata::GetCollectionID(void)
{
    return m_collectionID;
}

void TorcMetadata::SetCollectionID(int CollectionID)
{
    m_collectionID = CollectionID;
}
