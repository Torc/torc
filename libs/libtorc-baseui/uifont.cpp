/* Class UIFont
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

// Qt
#include <QFontMetricsF>

// Torc
#include "uiimage.h"
#include "uifont.h"

UIFont::UIFont()
  : TorcReferenceCounter(),
    m_brush(QColor(Qt::white)),
    m_hasShadow(false),
    m_shadowFixedBlur(0),
    m_hasOutline(false),
    m_outlineSize(0),
    m_relativeSize(0.0f),
    m_stretch(100),
    m_image(NULL),
    m_imageReady(false)
{
    CalculateHash();
}

UIFont::~UIFont()
{
    if (m_image)
        m_image->DownRef();
}

void UIFont::CopyFrom(UIFont *Other)
{
    if (!Other)
        return;

    SetFace(Other->GetFace());
    SetColor(Other->GetColor());
    SetBrush(Other->GetBrush());
    QPoint offset;
    QColor color;
    int    blur;
    quint16 size;
    Other->GetShadow(offset, color, blur);
    SetShadow(Other->HasShadow(), offset, color, blur);
    Other->GetOutline(color, size);
    SetOutline(Other->HasOutline(), color, size);
    SetOffset(Other->GetOffset());
    SetRelativeSize(Other->GetRelativeSize());
    SetSize(Other->GetFace().pixelSize());
    SetStretch(Other->GetStretch());
    SetImageFileName(Other->GetImageFileName());
    SetImageReady(Other->GetImageReady());
    UIImage* image = Other->GetImage();
    if (image)
    {
        SetImage(image);
        m_image->UpRef();
    }
}

QFont& UIFont::GetFace(void)
{
    return m_face;
}

QColor UIFont::GetColor(void) const
{
    return m_brush.color();
}

QBrush UIFont::GetBrush(void) const
{
    return m_brush;
}

void UIFont::GetShadow(QPoint &Offset, QColor &Color, int &FixedBlur) const
{
    Offset    = m_shadowOffset;
    Color     = m_shadowColor;
    FixedBlur = m_shadowFixedBlur;
}

void UIFont::GetOutline(QColor &Color,  quint16 &Size) const
{
    Color = m_outlineColor;
    Size  = m_outlineSize;
}

QPoint UIFont::GetOffset(void) const
{
    return m_offset;
}

QString UIFont::GetHash(void) const
{
    return m_hash;
}

float UIFont::GetRelativeSize(void) const
{
    return m_relativeSize;
}

int UIFont::GetStretch(void) const
{
    return m_stretch;
}

QString UIFont::GetImageFileName(void) const
{
    return m_imageFileName;
}

UIImage* UIFont::GetImage(void) const
{
    return m_image;
}

bool UIFont::GetImageReady(void) const
{
    return m_imageReady;
}

qreal UIFont::GetWidth(const QString &Text)
{
    QFontMetricsF metric(m_face);
    return metric.width(Text);
}

void UIFont::UpdateTexture(void)
{
    if (m_imageReady || !m_image)
        return;

    m_brush.setTextureImage(*(static_cast<QImage *> (m_image)));
    SetImageReady(true);
}

void UIFont::SetFace(const QFont &Face)
{
    m_face = Face;
    CalculateHash();
}

void UIFont::SetColor(const QColor &Color)
{
    m_brush.setColor(Color);
    CalculateHash();
}

void UIFont::SetImage(UIImage *Image)
{
    if (m_image)
        m_image->DownRef();
    m_image = Image;
    CalculateHash();
}

void UIFont::SetImageFileName(const QString &FileName)
{
    m_imageFileName = FileName;
}

void UIFont::SetImageReady(bool Ready)
{
    m_imageReady = Ready;
    CalculateHash();
}

void UIFont::SetBrush(const QBrush &Brush)
{
    m_brush = Brush;
    CalculateHash();
}

void UIFont::SetShadow(bool On, const QPoint &Offset, const QColor &Color, int FixedBlur)
{
    m_hasShadow    = On;
    m_shadowOffset = Offset;
    m_shadowColor  = Color;
    m_shadowFixedBlur = FixedBlur;
    CalculateHash();
}

void UIFont::SetOutline(bool On, const QColor &Color,  quint16 Size)
{
    m_hasOutline   = On;
    m_outlineColor = Color;
    m_outlineSize  = Size;
    CalculateHash();
}

void UIFont::SetOffset(const QPoint &Offset)
{
    m_offset = Offset;
    CalculateHash();
}

void UIFont::SetRelativeSize(float Size)
{
    m_relativeSize = Size;
    CalculateHash();
}

void UIFont::SetSize(int Size)
{
    if (Size > -1)
    {
        m_face.setPixelSize(Size);
        CalculateHash();
    }
}

void UIFont::SetStretch(int Stretch)
{
    m_stretch = Stretch;
    m_face.setStretch(m_stretch);
    CalculateHash();
}

bool UIFont::HasOutline(void)
{
    return m_hasOutline;
}

bool UIFont::HasShadow(void)
{
    return m_hasShadow;
}

void UIFont::CalculateHash(void)
{
    m_hash = QString("%1%2%3%4%5%6").arg(m_face.toString())
                                    .arg(m_brush.color().rgba())
                                    .arg(m_hasShadow)
                                    .arg(m_hasOutline)
                                    .arg(m_imageReady)
                                    .arg((quintptr)m_image);

    if (m_hasShadow)
    {
        m_hash += QString("%1%2%3%4")
                      .arg(m_shadowOffset.x())
                      .arg(m_shadowOffset.y())
                      .arg(m_shadowColor.name())
                      .arg(m_shadowFixedBlur);
    }

    if (m_hasOutline)
    {
        m_hash += QString("%1%2")
                      .arg(m_outlineColor.name())
                      .arg(m_outlineSize);
    }

    if (m_hasOutline)
        m_offset = QPoint(m_outlineSize, m_outlineSize);

    if (m_hasShadow && !m_hasOutline)
    {
        if (m_shadowOffset.x() < 0)
            m_offset.setX(-m_shadowOffset.x());
        if (m_shadowOffset.y() < 0)
            m_offset.setY(-m_shadowOffset.y());
    }
    if (m_hasShadow && m_hasOutline)
    {
        if (m_shadowOffset.x() < 0 && m_shadowOffset.x() < -m_outlineSize)
            m_offset.setX(-m_shadowOffset.x());
        if (m_shadowOffset.y() < 0 && m_shadowOffset.y() < -m_outlineSize)
            m_offset.setY(-m_shadowOffset.y());
    }
}
