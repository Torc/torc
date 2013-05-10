/* Class UIImageTracker
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
#include <QMutex>
#include <QThreadPool>

// Torc
#include "torccoreutils.h"
#include "torclogging.h"
#include "uiimage.h"
#include "uifont.h"
#include "uitextrenderer.h"
#include "uiimageloader.h"
#include "uishaperenderer.h"
#include "uiimagetracker.h"

UIImageTracker::UIImageTracker()
  : m_synchronous(false),
    m_hardwareCacheSize(0),
    m_softwareCacheSize(0),
    m_maxHardwareCacheSize(0),
    m_maxSoftwareCacheSize(0),
    m_maxExpireListSize(0),
    m_completedImagesLock(new QMutex(QMutex::Recursive))
{
    SetMaximumCacheSizes(96, 96, 1024);
}

UIImageTracker::~UIImageTracker()
{
    // wait for any outstanding render requests. The QRunnable will call this
    // object to notify completion.
    foreach (UIImage* image, m_outstandingImages)
        image->SetAbort(true);

    int count = 0;
    while (!m_outstandingImages.isEmpty() && count++ < 100)
        TorcUSleep(50000);

    if (!m_outstandingImages.isEmpty())
        LOG(VB_GENERAL, LOG_ERR, QString("Waited for 5 seconds and %1 images still not complete").arg(m_outstandingImages.size()));

    // Force deallocation of any remaining images
    ExpireImages(true);

    // Clean up any completed images that we haven't used
    // N.B. UIImages should already be deallocated...
    {
        QMutexLocker locker(m_completedImagesLock);
        QHashIterator<UIImage*,QImage*> it(m_completedImages);
        while (it.hasNext())
        {
            it.next();
            delete it.value();
        }

        m_completedImages.clear();
    }

    delete m_completedImagesLock;
    m_completedImagesLock = NULL;

    // Sanity check our ref counting
    if (!m_allocatedImages.isEmpty())
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("%1 images not de-allocated (cache size %2).")
                .arg(m_allocatedImages.size())
                .arg(m_softwareCacheSize));
    }
}

void UIImageTracker::SetMaximumCacheSizes(quint8 Hardware, quint8 Software,
                                          quint64 ExpirableItems)
{
    m_maxHardwareCacheSize  = 1024 * 1024 * Hardware;
    m_maxSoftwareCacheSize  = 1024 * 1024 * Software;
    m_maxExpireListSize = ExpirableItems;
    LOG(VB_GENERAL, LOG_INFO, QString("Cache sizes: Hardware %1Mb, Software %2Mb Software items %3")
        .arg(Hardware).arg(Software).arg(ExpirableItems));
}

UIImage* UIImageTracker::AllocateImage(const QString &Name,
                                       const QSize   &Size,
                                       const QString &FileName)
{
    bool file = false;

    if (!FileName.isEmpty() && !Size.isEmpty())
    {
        file = true;
        // Check whether we already have this image loaded at this size or larger
        if (m_allocatedFileImages.contains(FileName))
        {
            foreach (UIImage* image, m_allocatedImages)
            {
                if (image->GetFilename() == FileName &&
                    image->GetMaxSize().width() >= Size.width() &&
                    image->GetMaxSize().height() >= Size.height())
                {
                    image->UpRef();
                    return image;
                }
            }
        }
    }

    UIImage* image = new UIImage(this, Name, Size, FileName);
    if (image)
    {
        if (file)
            m_allocatedFileImages.append(FileName);

        m_allocatedImages.append(image);
        m_softwareCacheSize += image->byteCount();
    }
    return image;
}

void UIImageTracker::ReleaseImage(UIImage *Image)
{
    while (Image && m_allocatedImages.contains(Image))
    {
        m_allocatedImages.removeOne(Image);
        m_softwareCacheSize -= Image->byteCount();
        m_allocatedFileImages.removeOne(Image->GetFilename());
    }
}

UIImage* UIImageTracker::GetSimpleTextImage(const QString &Text,
                                            const QRectF  *Rect,
                                            UIFont        *Font,
                                            int            Flags,
                                            int            Blur)
{
    UIImage* image = NULL;

    QString hash = "S" + Text + Font->GetHash() +
                   QChar((uint)Rect->width()  & 0xffff) +
                   QChar((uint)Rect->height() & 0xffff) +
                   QChar(Flags & 0xffff) +
                   QChar((uint)Blur &0xff);

    if (m_imageHash.contains(hash))
    {
        image = m_imageHash[hash];

        UIImage::ImageState state = image->GetState();

        if (state == UIImage::ImageReleasedFromGPU)
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Image '%1' cache hit").arg(image->GetName()));
            m_expireList.removeOne(image);
            m_imageHash.remove(hash);
            image->DownRef();
        }
        else if (state != UIImage::ImageLoaded &&
                 state != UIImage::ImageUploadedToGPU)
        {
            return NULL;
        }
        else
        {
            m_expireList.removeOne(image);
            m_expireList.append(image);
            return image;
        }
    }

    image = AllocateImage(hash, QSize());
    m_imageHash.insert(hash, image);
    m_expireList.append(image);
    ExpireImages();

    UITextRenderer *render = new UITextRenderer(this, image, Text, Rect->size(), Font, Flags, Blur);
    m_outstandingImages << image;
    image->UpRef();
    if (m_synchronous)
    {
        render->run();
        delete render;
    }
    else
    {
        QThreadPool::globalInstance()->start(render, QThread::NormalPriority);
    }

    return image;
}

UIImage* UIImageTracker::GetShapeImage(UIShapePath *Path, const QRectF *Rect)
{
    if (!Path || !Rect)
        return NULL;

    QString hash = QString::number((quintptr)Path) +
            QChar((uint)Rect->width() & 0xffff) +
            QChar((uint)Rect->height() & 0xffff);

    UIImage* image = NULL;

    if (m_imageHash.contains(hash))
    {
        image = m_imageHash[hash];

        UIImage::ImageState state = image->GetState();

        if (state == UIImage::ImageReleasedFromGPU)
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Image '%1' cache hit").arg(image->GetName()));
            m_expireList.removeOne(image);
            m_imageHash.remove(hash);
            image->DownRef();
        }
        else if (state != UIImage::ImageLoaded &&
                 state != UIImage::ImageUploadedToGPU)
        {
            return NULL;
        }
        else
        {
            m_expireList.removeOne(image);
            m_expireList.append(image);
            return image;
        }
    }

    image = AllocateImage(hash, QSize());
    m_imageHash.insert(hash, image);
    m_expireList.append(image);
    ExpireImages();

    UIShapeRenderer *render = new UIShapeRenderer(this, image, Path, Rect->size());
    m_outstandingImages << image;
    image->UpRef();

    if (m_synchronous)
    {
        render->run();
        delete render;
    }
    else
    {
        QThreadPool::globalInstance()->start(render, QThread::NormalPriority);
    }

    return image;
}

void UIImageTracker::LoadImageFromFile(UIImage *Image)
{
    if (!Image)
        return;

    UIImage::ImageState state = Image->GetState();

    if (state == UIImage::ImageLoaded ||
        state == UIImage::ImageUploadedToGPU)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Image already loaded. Ignoring");
        return;
    }

    if (state == UIImage::ImageLoading)
    {
        LOG(VB_GENERAL, LOG_ERR, "Image already loading. Not reloading.");
        return;
    }

    UIImageLoader *loader = new UIImageLoader(this, Image);
    m_outstandingImages << Image;
    Image->UpRef();

    if (m_synchronous)
    {
        loader->run();
        delete loader;
    }
    else
    {
        QThreadPool::globalInstance()->start(loader, QThread::NormalPriority);
    }
}

void UIImageTracker::ImageCompleted(UIImage *Image, QImage *Completed)
{
    m_completedImagesLock->lock();

    if (m_completedImages.contains(Image))
    {
        LOG(VB_GENERAL, LOG_ERR, "Image already loaded. Discarding..");
        delete Completed;
    }
    else
    {
        m_completedImages.insert(Image, Completed);
    }

    m_completedImagesLock->unlock();
}

void UIImageTracker::UpdateImages(void)
{
    m_completedImagesLock->lock();
    QHash<UIImage*,QImage*> images = m_completedImages;
    m_completedImages.clear();
    m_completedImagesLock->unlock();

    QHash<UIImage*,QImage*>::iterator it = images.begin();
    for ( ; it != images.end(); ++it)
    {
        m_softwareCacheSize -= it.key()->byteCount();
        if (it.value())
        {
            QImage& image = *(it.value());
            it.key()->Assign(image);
        }

        it.key()->DownRef();
        m_outstandingImages.removeAll(it.key());

        delete it.value();
        m_softwareCacheSize += it.key()->byteCount();
    }
}

void UIImageTracker::ExpireImages(bool ExpireAll)
{
    while (((m_softwareCacheSize >= (ExpireAll ? 0 : m_maxSoftwareCacheSize)) &&
           !m_expireList.isEmpty()) || (m_expireList.size() > m_maxExpireListSize))
    {
        UIImage* image = m_expireList.takeFirst();
        if (m_imageHash.contains(image->GetName()))
        {
            m_imageHash.take(image->GetName());
            image->DownRef();
        }
    }
}
