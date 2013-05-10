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

// Torc
#include "torcconfig.h"
#include "torclogging.h"
#include "torcbuffer.h"
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

    QImage *image = NULL;
    QString filename;

    if (m_image->GetRawSVGData())
    {
#if defined(CONFIG_QTSVG)
        image = new QImage(m_image->GetMaxSize(), QImage::Format_ARGB32_Premultiplied);
        QPainter painter(image);
        QSvgRenderer renderer(*m_image->GetRawSVGData());
        if (renderer.isValid())
        {
            renderer.render(&painter);
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to render raw SVG");
        }
#else
        LOG(VB_GENERAL, LOG_WARNING, "No SVG support - unable to render raw SVG");
#endif
    }
    else
    {
        filename = m_image->GetFilename();

        if (filename.isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR, "No file name.");
            return;
        }

        // load it
        QScopedPointer<TorcBuffer> file(TorcBuffer::Create(filename, m_image->GetAbort()));
        if (!file.data())
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to open '%1'").arg(filename));
            return;
        }

        QByteArray content(file.data()->ReadAll(20000/*20 seconds*/));

        if (filename.endsWith("svg", Qt::CaseInsensitive))
        {
#if defined(CONFIG_QTSVG)
            image = new QImage(m_image->GetMaxSize(), QImage::Format_ARGB32_Premultiplied);
            QPainter painter(image);
            QSvgRenderer renderer(content);
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
            image = new QImage(QImage::fromData(content, "png"));
        }
    }

    if (image->isNull())
        LOG(VB_GENERAL, LOG_WARNING, QString("Failed to load '%1'").arg(filename));
    else if (image->width() > m_image->GetMaxSize().width() || image->height() > m_image->GetMaxSize().height())
        *image = image->scaled(m_image->GetMaxSize(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    m_parent->ImageCompleted(m_image, image);
}
