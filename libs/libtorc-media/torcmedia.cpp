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

TorcMedia::TorcMedia()
  : name(QString()),
    url(QString()),
    type(UnknownType),
    source(MediaSourceLocal),
    metadata(NULL)
{
}

TorcMedia::TorcMedia(const QString &Name, const QString &URL, MediaType Type,
                     MediaSource Source, TorcMetadata *Metadata)
  : name(Name),
    url(URL),
    type(Type),
    source(Source),
    metadata(Metadata)
{
}

TorcMedia::~TorcMedia()
{
    if (metadata)
        delete metadata;
}

QString TorcMedia::GetName(void)
{
    return name;
}

QString TorcMedia::GetURL(void)
{
    return url;
}

TorcMedia::MediaType TorcMedia::GetMediaType(void)
{
    return type;
}

TorcMedia::MediaSource TorcMedia::GetMediaSource(void)
{
    return source;
}

TorcMetadata* TorcMedia::GetMetadata(void)
{
    return metadata;
}

void TorcMedia::SetName(const QString &Name)
{
    if (name != Name)
    {
        name = Name;
        emit nameChanged(name);
    }
}

void TorcMedia::SetURL(const QString &URL)
{
    if (url != URL)
    {
        url = URL;
        emit urlChanged(url);
    }
}

void TorcMedia::SetMediaType(MediaType Type)
{
    if (type != Type)
    {
        type = Type;
        emit typeChanged(type);
    }
}

void TorcMedia::SetMediaSource(MediaSource Source)
{
    if (source != Source)
    {
        source = Source;
        emit sourceChanged(source);
    }
}

void TorcMedia::SetMetadata(TorcMetadata *Metadata)
{
    if (metadata != Metadata)
    {
        if (metadata)
            delete metadata;

        metadata = Metadata;
        emit metadataChanged(metadata);
    }
}

TorcMediaDescription::TorcMediaDescription()
  : name(),
    url(),
    type(TorcMedia::UnknownType),
    source(TorcMedia::MediaSourceLocal),
    metadata(NULL)
{
}

TorcMediaDescription::TorcMediaDescription(const QString &Name, const QString &URL, TorcMedia::MediaType Type,
                                           TorcMedia::MediaSource Source, TorcMetadata *Metadata)
  : name(Name),
    url(URL),
    type(Type),
    source(Source),
    metadata(Metadata)
{
}
