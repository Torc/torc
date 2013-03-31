/* Class UITextRenderer
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
#include <QThread>
#include <QPainter>

// Torc
#include "uiimage.h"
#include "uifont.h"
#include "uiimagetracker.h"
#include "uitextrenderer.h"

UITextRenderer::UITextRenderer(UIImageTracker *Parent, UIImage *Image,
                               QString Text,           QSizeF  Size,
                               const UIFont *Font,     int     Flags,
                               int Blur)
  : QRunnable(),
    m_parent(Parent),
    m_image(Image),
    m_text(Text),
    m_size(Size),
    m_flags(Flags),
    m_blur(Blur)
{
    m_image->SetState(UIImage::ImageLoading);
    m_font = const_cast<UIFont*>(Font);
    m_image->UpRef();
    m_font->UpRef();
}

UITextRenderer::~UITextRenderer()
{
    m_font->DownRef();
    m_image->DownRef();
}

void UITextRenderer::RenderText(QPaintDevice *Device, const QString &Text,
                                UIFont *Font, int Flags, int Blur)
{
    if (!Device || !Font)
        return;

    QPoint offset = Font->GetOffset();
    QColor outlinecolor;
    quint16 outlinesize  = 0;
    QFont font    = Font->GetFace();
    bool invert   = Flags & FONT_INVERT;
    font.setStyleStrategy(QFont::OpenGLCompatible);

    if (Font->HasOutline())
        Font->GetOutline(outlinecolor, outlinesize);

    QPainter painter(Device);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.setFont(font);

    if (invert)
        painter.fillRect(0, 0, Device->width(), Device->height(), Font->GetBrush());
    else
        painter.fillRect(0, 0, Device->width(), Device->height(), QColor(0, 0, 0, 0));

    if (Font->HasShadow() && !invert)
    {
        QPoint  shadowoffset;
        QColor  shadowcolor;
        int     blur;

        Font->GetShadow(shadowoffset, shadowcolor, blur);

        QRect rect(shadowoffset.x() + offset.x(),
                   shadowoffset.y() + offset.y(),
                   Device->width(),
                   Device->height());

        painter.setPen(shadowcolor);
        painter.drawText(rect, Flags, Text);

        if (blur)
        {
            QImage* image = static_cast<QImage*>(Device);
            if (image)
                UIImage::Blur(image, blur);
        }
    }

    if (Font->HasOutline())
    {
        // FIXME: this is seriously inefficient

        QRect rect(-outlinesize + offset.x(),
                   -outlinesize + offset.y(),
                   Device->width(),
                   Device->height());

        painter.setPen(outlinecolor);
        painter.drawText(rect, Flags, Text);

        for (int i = (0 - outlinesize + 1); i <= outlinesize; i++)
        {
            rect.translate(1, 0);
            painter.drawText(rect, Flags, Text);
        }

        for (int i = (0 - outlinesize + 1); i <= outlinesize; i++)
        {
            rect.translate(0, 1);
            painter.drawText(rect, Flags, Text);
        }

        for (int i = (0 - outlinesize + 1); i <= outlinesize; i++)
        {
            rect.translate(-1, 0);
            painter.drawText(rect, Flags, Text);
        }

        for (int i = (0 - outlinesize + 1); i <= outlinesize; i++)
        {
            rect.translate(0, -1);
            painter.drawText(rect, Flags, Text);
        }
    }

    if (invert)
        painter.setPen(QColor(0,0,0,0));
    else
        painter.setPen(QPen(Font->GetBrush(), 0));
    painter.drawText(offset.x(), offset.y(),
                     Device->width(), Device->height(),
                     Flags, Text);
    painter.end();

    if (Blur)
    {
        QImage* image = static_cast<QImage*>(Device);
        if (image)
            UIImage::Blur(image, Blur);
    }
}

void UITextRenderer::run()
{
    QSize size((int)m_size.width(), (int)m_size.height());
    QImage *image = new QImage(size, QImage::Format_ARGB32_Premultiplied);

    if (!image)
        return;

    RenderText(image, m_text, m_font, m_flags, m_blur);

    m_parent->ImageCompleted(m_image, image);
}

