/* Class UIImageLoader
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

// TODO
// - preserve aspect ratio
// - remote files (QImageLoader etc)

// Qt
#include <QFile>

// Torc
#include "torclogging.h"
#include "uiimage.h"
#include "uiimagetracker.h"
#include "uiimageloader.h"

#define LOC QString("ImageLoader: ")

UIImageLoader::UIImageLoader(UIImageTracker *Parent, UIImage *Image)
  : QRunnable(),
    m_parent(Parent),
    m_image(Image)
{
    m_image->SetState(UIImage::ImageLoading);
    m_image->UpRef();
}

UIImageLoader::~UIImageLoader()
{
    m_image->DownRef();
}

void UIImageLoader::run(void)
{
    if (!m_image)
        return;

    QString filename = m_image->GetFilename();

    if (filename.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No file name.");
        return;
    }

    if (!QFile::exists(filename))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Image '%1' does not exist.")
            .arg(filename));
        return;
    }

    QImage *image = new QImage(filename, "png");

    if (image->isNull())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Failed to load '%1'").arg(filename));
    }
    else if (image->width() > m_image->GetMaxSize().width() ||
             image->height() > m_image->GetMaxSize().height())
    {
        *image = image->scaled(m_image->GetMaxSize(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    m_parent->ImageCompleted(m_image, image);
}
