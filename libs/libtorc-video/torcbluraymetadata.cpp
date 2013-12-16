/* Class TorcBlurayMetadata
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

// Qt
#include <QDir>
#include <QDirIterator>
#include <QDomDocument>

// Torc
#include "torclogging.h"
#include "torcbluraymetadata.h"

extern "C" {
#include "libbluray/src/libbluray/register.h"
}

/*! \class TorcBlurayMetadata
 *  \brief Locate and parse bluray metadata information.
 *
 * Libbluray requires libmxl2 to parse metadata. To avoid an extra library dependency,
 * use Qt's xml functionality instead.
 *
 * \todo This will need fixing when remote file access is added.
*/
TorcBlurayMetadata::TorcBlurayMetadata(const QString &Path)
{
    LOG(VB_GENERAL, LOG_INFO, QString("Retrieving metadata from '%1'").arg(Path));

    QString directory = Path + "/BDMV/META/DL/";
    QStringList namefilters("bdmt_*.xml");
    QDirIterator it(directory, namefilters, QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot | QDir::Readable);
    while (it.hasNext())
    {
        it.next();

        QDomDocument doc;
        QFile file(it.filePath());

        if (!file.open(QIODevice::ReadOnly))
            continue;

        if (!doc.setContent(&file))
        {
            file.close();
            continue;
        }

        QDomNodeList discinfos = doc.elementsByTagName("di:discinfo");
        for (int i = 0; i < discinfos.size(); ++i)
        {
            // create a new meta_dl entry
            meta_dl* metadata = new meta_dl;
            memset(metadata, 0, sizeof(meta_dl));

            // for consistency with libbluray
            metadata->di_set_number = metadata->di_num_sets = -1;

            // language comes from the filename
            QByteArray language(it.fileName().mid(5, 3).toLower().toLocal8Bit());
            strncpy(metadata->language_code, language.data(), 3);
            metadata->language_code[3] = '\0';

            // filename
            metadata->filename = strdup((const char*)it.fileName().toLocal8Bit().data());

            QDomNodeList ids = discinfos.at(i).childNodes();
            for (int j = 0; j < ids.size(); ++j)
            {
                QString nodename = ids.at(j).nodeName();
                if (nodename == QString("di:title"))
                {
                    QDomElement name        = ids.at(j).namedItem("di:name").toElement();
                    QDomElement alternative = ids.at(j).namedItem("di:alternative").toElement();
                    QDomElement numsets     = ids.at(j).namedItem("di:numSets").toElement();
                    QDomElement setnumber   = ids.at(j).namedItem("di:setNumber").toElement();

                    if (!name.isNull())
                        metadata->di_name = strdup((const char*)name.text().toLocal8Bit().data());

                    if (!alternative.isNull())
                        metadata->di_alternative = strdup((const char*)alternative.text().toLocal8Bit().data());

                    if (!numsets.isNull())
                    {
                        bool ok = false;
                        int num = numsets.text().toInt(&ok);
                        if (ok)
                            metadata->di_num_sets = num & 0xff;
                    }

                    if (!setnumber.isNull())
                    {
                        bool ok = false;
                        int num = setnumber.text().toInt(&ok);
                        if (ok)
                            metadata->di_set_number = num & 0xff;
                    }
                }
                else if (nodename == QString("di:description"))
                {
                    // thumbnails
                    QDomElement e = ids.at(j).toElement();
                    QDomNodeList thumbnails = e.elementsByTagName("di:thumbnail");

                    if (!thumbnails.isEmpty())
                    {
                        metadata->thumb_count = thumbnails.size();
                        metadata->thumbnails  = new META_THUMBNAIL[thumbnails.size()];

                        for (int k = 0; k < thumbnails.size(); ++k)
                        {
                            QDomElement el = thumbnails.at(k).toElement();
                            QString href = el.attribute("href");
                            QString size = el.attribute("size");

                            if (!href.isEmpty())
                                metadata->thumbnails[k].path = strdup((const char*)href.toLocal8Bit().data());

                            QStringList values = size.split("x", QString::SkipEmptyParts);
                            if (values.size() == 2)
                            {
                                bool ok = false;
                                int x = values[0].toInt(&ok);
                                if (ok)
                                {
                                    int y = values[1].toInt(&ok);
                                    if (ok)
                                    {
                                        metadata->thumbnails[k].xres = x;
                                        metadata->thumbnails[k].yres = y;
                                    }
                                }
                            }
                            else
                            {
                                // yes, -1. Just being consistent libbluray...
                                metadata->thumbnails[k].xres = -1;
                                metadata->thumbnails[k].yres = -1;
                            }
                        }
                    }

                    // table of contents
                    e = ids.at(j).firstChildElement("di:tableOfContents");

                    if (!e.isNull())
                    {
                        QDomNodeList titles = e.elementsByTagName("di:titleName");

                        if (!titles.isEmpty())
                        {
                            metadata->toc_count   = titles.size();
                            metadata->toc_entries = new META_TITLE[titles.size()];

                            for (int k = 0; k < titles.size(); ++k)
                            {
                                QDomElement el  = titles.at(k).toElement();
                                QString snumber = el.attribute("titleNumber");
                                QString sname   = el.text();

                                bool ok = false;
                                quint32 number = snumber.toInt(&ok);
                                if (ok)
                                {
                                    metadata->toc_entries[k].title_number = number;
                                    metadata->toc_entries[k].title_name   = strdup((const char*)sname.toLocal8Bit().data());
                                }
                            }
                        }
                    }
                }
            }

            m_metadata.append(metadata);
        }

        file.close();
    }
}

TorcBlurayMetadata::~TorcBlurayMetadata()
{
    while (!m_metadata.isEmpty())
    {
        meta_dl* metadata = m_metadata.takeLast();

        for (int i = 0; i < metadata->toc_count; ++i)
            free(metadata->toc_entries[i].title_name);

        for (int i = 0; i < metadata->thumb_count; ++i)
            free(metadata->thumbnails[i].path);

        delete [] metadata->thumbnails;
        delete [] metadata->toc_entries;

        free(metadata->di_alternative);
        free(metadata->di_name);
        free(metadata->filename);

        delete metadata;
    }
}

meta_dl* TorcBlurayMetadata::GetMetadata(const char *Language)
{
    // try to match requested language first
    if (Language)
        foreach (meta_dl* metadata, m_metadata)
            if (strncmp(Language, metadata->language_code, 3) == 0)
                return metadata;

    // then the libbluray default
    foreach (meta_dl* metadata, m_metadata)
        if (strncmp(DEFAULT_LANGUAGE, metadata->language_code, 3) == 0)
            return metadata;

    // return the first
    if (!m_metadata.isEmpty())
        return m_metadata.at(0);

    // return nothing
    return NULL;
}
