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
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#include "mime/qmimedatabase.h"
#else
#include <QMimeDatabase>
#endif

//Torc
#include "torcmime.h"

QMimeDatabase *gMimeDatabase = new QMimeDatabase();

QString TorcMime::MimeTypeForData(const QByteArray &Data)
{
    return gMimeDatabase->mimeTypeForData(Data).name();
}

QString TorcMime::MimeTypeForData(QIODevice *Device)
{
    if (Device)
        return gMimeDatabase->mimeTypeForData(Device).name();
    return QString();
}

QString TorcMime::MimeTypeForFileNameAndData(const QString &FileName, QIODevice *Device)
{
    if (FileName.isEmpty() || !Device)
        return QString();

    return gMimeDatabase->mimeTypeForFileNameAndData(FileName, Device).name();
}

QString TorcMime::MimeTypeForFileNameAndData(const QString &FileName, const QByteArray &Data)
{
    if (FileName.isEmpty() || Data.isEmpty())
        return QString();

    return gMimeDatabase->mimeTypeForFileNameAndData(FileName, Data).name();
}

QString TorcMime::MimeTypeForName(const QString &Name)
{
    return gMimeDatabase->mimeTypeForName(Name).name();
}

QString TorcMime::MimeTypeForUrl(const QUrl &Url)
{
    return gMimeDatabase->mimeTypeForUrl(Url).name();
}

QStringList TorcMime::MimeTypeForFileName(const QString &FileName)
{
    QStringList result;

    QList<QMimeType> types = gMimeDatabase->mimeTypesForFileName(FileName);
    foreach (QMimeType type, types)
        result << type.name();

    return result;
}

QStringList TorcMime::ExtensionsForType(const QString &Type)
{
    QStringList extensions;

    QList<QMimeType> mimes = gMimeDatabase->allMimeTypes();
    foreach (QMimeType mime, mimes)
        if (mime.name().startsWith(Type, Qt::CaseInsensitive))
            extensions.append(mime.suffixes());

    return extensions;
}
