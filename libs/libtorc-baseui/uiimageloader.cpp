/* Class UIImageLoader
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

// Qt
#include <QFile>

// Torc
#include "torcconfig.h"
#include "torclogging.h"
#include "uiimage.h"
#include "uiimagetracker.h"
#include "uiimageloader.h"

#if defined(CONFIG_QTSVG)
#include <QPainter>
#include <QSvgRenderer>
#endif

/*! \class UIImageLoader
 *  \brief A simple asynchronous image loader.
 *
 * UIImageLoader loads an image file into a UIImage from within a QRunnable.
 * It holds a reference to the UIImage until it is complete and notifies the parent
 * UIImageTracker when it is finished.
 *
 * Image format is currently restricted to png.
 *
 * \sa UIImageTracker
 * \sa UIShapeRenderer
 * \sa UITextRenderer
 *
 * \todo Preserve aspect ratio
 * \todo Remote files
 * \todo Check support for formats other than png (do they load?)
*/

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
        LOG(VB_GENERAL, LOG_ERR, "No file name.");
        return;
    }

    if (!QFile::exists(filename))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Image '%1' does not exist.").arg(filename));
        return;
    }

    QImage *image = NULL;

    if (filename.endsWith("svg", Qt::CaseInsensitive))
    {
#if defined(CONFIG_QTSVG)
        image = new QImage(m_image->GetMaxSize(), QImage::Format_ARGB32_Premultiplied);
        QPainter painter(image);
        QSvgRenderer renderer(filename);
        if (renderer.isValid())
        {
            renderer.render(&painter);
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to render '%1'").arg(filename));
        }
#else
        LOG(VB_GENERAL, LOG_WARNING, QString("No SVG support - unable to load '%1'").arg(filename));
#endif
    }
    else

    {
        image = new QImage(filename, "png");
    }

    if (image->isNull())
        LOG(VB_GENERAL, LOG_WARNING, QString("Failed to load '%1'").arg(filename));
    else if (image->width() > m_image->GetMaxSize().width() || image->height() > m_image->GetMaxSize().height())
        *image = image->scaled(m_image->GetMaxSize(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    m_parent->ImageCompleted(m_image, image);
}
