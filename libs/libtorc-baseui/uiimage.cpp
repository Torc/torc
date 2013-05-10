/* Class UIImage
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
#include "torclogging.h"
#include "uiimagetracker.h"
#include "uiimage.h"

UIImage::UIImage(UIImageTracker *Parent, const QString &Name, const QSize &MaxSize,
                 const QString &FileName)
  : TorcReferenceCounter(),
    m_parent(Parent),
    m_state(ImageNull),
    m_name(Name),
    m_filename(FileName),
    m_sizeF(QSizeF()),
    m_maxSize(MaxSize),
    m_abort(0),
    m_rawSVGData(NULL)
{
}

UIImage::~UIImage()
{
    if (m_parent)
        m_parent->ReleaseImage(this);
}

QString UIImage::GetName(void)
{
    return m_name;
}

QString UIImage::GetFilename(void)
{
    return m_filename;
}

void UIImage::SetState(ImageState State)
{
    m_state = State;
}

UIImage::ImageState UIImage::GetState(void)
{
    return m_state;
}

void UIImage::Assign(const QImage &Image)
{
    QImage* current = static_cast<QImage*>(this);
    if (current)
        *current = Image;
    else
        LOG(VB_GENERAL, LOG_ERR, "Failed to assign image");
    m_state = ImageLoaded;
    m_sizeF = QSizeF((qreal)size().width(), (qreal)size().height());
}

void UIImage::FreeMem(void)
{
    QImage empty;
    *(static_cast<QImage *> (this)) = empty;
    SetState(ImageUploadedToGPU);
}

QSizeF* UIImage::GetSizeF(void)
{
    return &m_sizeF;
}

void UIImage::SetSizeF(const QSizeF &Size)
{
    m_sizeF = Size;
}

QSize& UIImage::GetMaxSize(void)
{
    return m_maxSize;
}

void UIImage::Blur(QImage* Image, int Radius)
{
    if (!Image)
        return;

    if (Image->format() != QImage::Format_ARGB32_Premultiplied)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Image not premultiplied - ignoring blur");
        return;
    }

    int tab[] = { 14, 10, 8, 6, 5, 5, 4, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2 };
    int alpha = (Radius < 1) ? 16 : (Radius > 17) ? 1 : tab[Radius-1];


    int rows = Image->height() - 1;
    int cols = Image->width() - 1;

    int bpl = Image->bytesPerLine();
    int rgba[4];
    unsigned char* p;

    for (int col = 0; col <= cols; col++)
    {
        p = Image->scanLine(0) + col * 4;
        for (int i = 0; i <= 3; i++)
            rgba[i] = p[i] << 4;

        p += bpl;
        for (int j = 0; j < rows; j++, p += bpl)
            for (int i = 0; i <= 3; i++)
                p[i] = (rgba[i] += ((p[i] << 4) - rgba[i]) * alpha / 16) >> 4;
    }

    for (int row = 0; row <= rows; row++)
    {
        p = Image->scanLine(row) + 0 * 4;
        for (int i = 0; i <= 3; i++)
            rgba[i] = p[i] << 4;

        p += 4;
        for (int j = 0; j < cols; j++, p += 4)
            for (int i = 0; i <= 3; i++)
                p[i] = (rgba[i] += ((p[i] << 4) - rgba[i]) * alpha / 16) >> 4;
    }

    for (int col = 0; col <= cols; col++)
    {
        p = Image->scanLine(rows) + col * 4;
        for (int i = 0; i <= 3; i++)
            rgba[i] = p[i] << 4;

        p -= bpl;
        for (int j = 0; j < rows; j++, p -= bpl)
            for (int i = 0; i <= 3; i++)
                p[i] = (rgba[i] += ((p[i] << 4) - rgba[i]) * alpha / 16) >> 4;
    }

    for (int row = 0; row <= rows; row++)
    {
        p = Image->scanLine(row) + cols * 4;
        for (int i = 0; i <= 3; i++)
            rgba[i] = p[i] << 4;

        p -= 4;
        for (int j = 0; j < cols; j++, p -= 4)
            for (int i = 0; i <= 3; i++)
                p[i] = (rgba[i] += ((p[i] << 4) - rgba[i]) * alpha / 16) >> 4;
    }
}

int* UIImage::GetAbort(void)
{
    return &m_abort;
}

void UIImage::SetAbort(bool Abort)
{
    m_abort = Abort ? 1 : 0;
}

void UIImage::SetRawSVGData(QByteArray *Data)
{
    m_rawSVGData = Data;
}

QByteArray* UIImage::GetRawSVGData(void)
{
    return m_rawSVGData;
}
