/* Class TorcMime
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

#include <QtGlobal>
#include <QMimeDatabase>

//Torc
#include "torcmime.h"

/// Pointer to the static singleton QMimeDatabase
QMimeDatabase *gMimeDatabase = new QMimeDatabase();

/*! \class TorcMime
 *  \brief A wrapper around QMimeDatabase.
 *
 * TorcMime provides a static interface to the database of MIME types
 * provided by QMimeDatabase.
 *
 * \note QMimeDatabase is only available with Qt 5.0 and greater. For Qt 4.8 support, a backport
 * of the QMimeDatabase code is included. This will be removed when Qt 5.0 becomes the minimum
 * requirement.
*/

/// \brief Return a MIME type for the media described by Data.
QString TorcMime::MimeTypeForData(const QByteArray &Data)
{
    return gMimeDatabase->mimeTypeForData(Data).name();
}

/// \brief Return a MIME type for the media accessed via Device.
QString TorcMime::MimeTypeForData(QIODevice *Device)
{
    if (Device)
        return gMimeDatabase->mimeTypeForData(Device).name();
    return QString();
}

/// \brief Return a MIME type for the media described by FileName and Device.
QString TorcMime::MimeTypeForFileNameAndData(const QString &FileName, QIODevice *Device)
{
    if (FileName.isEmpty() || !Device)
        return QString();

    return gMimeDatabase->mimeTypeForFileNameAndData(FileName, Device).name();
}

/// \brief Return a MIME type for the media described by FileName and Data.
QString TorcMime::MimeTypeForFileNameAndData(const QString &FileName, const QByteArray &Data)
{
    if (FileName.isEmpty() || Data.isEmpty())
        return QString();

    return gMimeDatabase->mimeTypeForFileNameAndData(FileName, Data).name();
}

/// \brief Return a MIME type for the file named Name.
QString TorcMime::MimeTypeForName(const QString &Name)
{
    return gMimeDatabase->mimeTypeForName(Name).name();
}

/// \brief Return a MIME type for the media pointed to by Url.
QString TorcMime::MimeTypeForUrl(const QUrl &Url)
{
    return gMimeDatabase->mimeTypeForUrl(Url).name();
}

/// \brief Return a list of possible MIME types for the file named Name.
QStringList TorcMime::MimeTypeForFileName(const QString &FileName)
{
    QStringList result;

    QList<QMimeType> types = gMimeDatabase->mimeTypesForFileName(FileName);
    foreach (QMimeType type, types)
        result << type.name();

    (void)result.removeDuplicates();
    return result;
}

/*! \brief Returns a list of known file extensions for a given top level MIME type.
 *
 * For example, passing "audio" as the type will return mp3, aac, ogg, wav etc.
*/
QStringList TorcMime::ExtensionsForType(const QString &Type)
{
    QStringList extensions;

    QList<QMimeType> mimes = gMimeDatabase->allMimeTypes();
    foreach (QMimeType mime, mimes)
        if (mime.name().startsWith(Type, Qt::CaseInsensitive))
            extensions.append(mime.suffixes());

    (void)extensions.removeDuplicates();
    return extensions;
}
