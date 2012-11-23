/* Class VideoRenderer
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

// Torc
#include "torclogging.h"
#include "uidisplay.h"
#include "uiwindow.h"
#include "videoframe.h"
#include "videorenderer.h"

/*! \class VideoRenderer
 *  \brief The base class for displaying video in the Torc UI
 *
 * \sa VideoRendererOpenGL
 *
 * \todo Extend positioning for zoom etc
*/

VideoRenderer::VideoRenderer(UIWindow *Window)
{
    m_display = dynamic_cast<UIDisplay*>(Window);

    if (!m_display)
        LOG(VB_GENERAL, LOG_ERR, "Failed to find display object");
}

VideoRenderer::~VideoRenderer()
{
}

bool VideoRenderer::UpdatePosition(VideoFrame* Frame)
{
    qreal avframePAR   = Frame->m_pixelAspectRatio;
    qreal displayPAR   = m_display->GetPixelAspectRatio();
    QSize displaysize  = m_display->GetGeometry();
    qreal width        = Frame->m_rawWidth * avframePAR * displayPAR;
    qreal height       = Frame->m_rawHeight;
    qreal widthfactor  = (qreal)displaysize.width() / width;
    qreal heightfactor = (qreal)displaysize.height() / height;

    qreal left = 0.0f;
    qreal top  = 0.0f;

    if (widthfactor < heightfactor)
    {
        width *= widthfactor;
        height *= widthfactor;
        top = (displaysize.height() - height) / 2.0f;
    }
    else
    {
        width *= heightfactor;
        height *= heightfactor;
        left = (displaysize.width() - width) / 2.0f;
    }

    LOG(VB_PLAYBACK, LOG_DEBUG, QString("Display rect: %1x%2+%3+%4 %5")
        .arg(width).arg(height).arg(left).arg(top).arg(displayPAR));

    QRectF newrect(left, top, width, height);
    bool changed = newrect != m_presentationRect;
    m_presentationRect = newrect;
    return changed;
}

VideoRenderer* VideoRenderer::Create(void)
{
    VideoRenderer* render = NULL;

    RenderFactory* factory = RenderFactory::GetRenderFactory();
    for ( ; factory; factory = factory->NextFactory())
    {
        render = factory->Create();
        if (render)
            break;
    }

    if (!render)
        LOG(VB_GENERAL, LOG_ERR, "Failed to create video renderer");

    return render;
}

RenderFactory* RenderFactory::gRenderFactory = NULL;

RenderFactory::RenderFactory()
{
    nextRenderFactory = gRenderFactory;
    gRenderFactory = this;
}

RenderFactory::~RenderFactory()
{
}

RenderFactory* RenderFactory::GetRenderFactory(void)
{
    return gRenderFactory;
}

RenderFactory* RenderFactory::NextFactory(void) const
{
    return nextRenderFactory;
}