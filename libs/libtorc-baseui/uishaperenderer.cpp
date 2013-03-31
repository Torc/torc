/* Class UIShapeRenderer
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
#include <QPainter>
#include <QPaintDevice>
#include <QPainterPath>

// Torc
#include "uiimage.h"
#include "uishapepath.h"
#include "uiimagetracker.h"
#include "uishaperenderer.h"

UIShapeRenderer::UIShapeRenderer(UIImageTracker *Parent, UIImage *Image,
                                 UIShapePath *Path, QSizeF Size)
  : QRunnable(),
    m_parent(Parent),
    m_image(Image),
    m_path(Path),
    m_size(Size)
{
    m_image->SetState(UIImage::ImageLoading);
    m_image->UpRef();
    m_path->UpRef();
}

UIShapeRenderer::~UIShapeRenderer()
{
    m_path->DownRef();
    m_image->DownRef();
}

void UIShapeRenderer::RenderShape(QPaintDevice *Device, UIShapePath *Path)
{
    if (Path)
        Path->RenderPath(Device);
}

void UIShapeRenderer::run(void)
{
    QSize size((int)m_size.width(), (int)m_size.height());
    QImage *image = new QImage(size, QImage::Format_ARGB32_Premultiplied);
    image->fill(0);

    if (!image)
        return;

    RenderShape(image, m_path);

    m_parent->ImageCompleted(m_image, image);
}
