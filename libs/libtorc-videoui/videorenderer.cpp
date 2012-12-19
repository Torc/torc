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
#include "torclocalcontext.h"
#include "torclogging.h"
#include "uidisplay.h"
#include "uiwindow.h"
#include "videoframe.h"
#include "videocolourspace.h"
#include "videorenderer.h"

/*! \class VideoRenderer
 *  \brief The base class for displaying video in the Torc UI
 *
 * \sa VideoRendererOpenGL
 *
 * \todo Handle frame rate doubling for interlaced material
 * \todo Extend positioning for zoom etc
*/

VideoRenderer::VideoRenderer(VideoColourSpace *ColourSpace, UIWindow *Window)
  : m_window(Window),
    m_colourSpace(ColourSpace),
    m_wantHighQualityScaling(false),
    m_allowHighQualityScaling(false),
    m_usingHighQualityScaling(false)
{
    m_display = dynamic_cast<UIDisplay*>(Window);

    if (!m_display)
        LOG(VB_GENERAL, LOG_ERR, "Failed to find display object");

    m_wantHighQualityScaling = gLocalContext->GetSetting(TORC_GUI + "BicubicScaling", false);
}

VideoRenderer::~VideoRenderer()
{
}

void VideoRenderer::PlaybackFinished(void)
{
    ResetOutput();

    if (m_display)
        m_window->SetRefreshRate(m_display->GetDefaultRefreshRate(), m_display->GetDefaultMode());
}

void VideoRenderer::ResetOutput(void)
{
    m_usingHighQualityScaling = false;
}

void VideoRenderer::UpdateRefreshRate(VideoFrame* Frame)
{
    if (!m_display || !Frame)
        return;

    // CanHandleVideoRate will cache the result of repeated calls
    int modeindex = -1;
    if (m_display->CanHandleVideoRate(Frame->m_frameRate, modeindex))
        m_window->SetRefreshRate(Frame->m_frameRate, modeindex);
}

bool VideoRenderer::UpdatePosition(VideoFrame* Frame)
{
    if (!m_display || !Frame)
        return false;

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

    QRectF newrect(left, top, width, height);
    bool changed = newrect != m_presentationRect;
    m_presentationRect = newrect;

    if (changed)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Display rect: %1x%2+%3+%4 %5")
            .arg(width).arg(height).arg(left).arg(top).arg(displayPAR));
    }

    // enable/disable high quality scaling
    if (m_wantHighQualityScaling && m_allowHighQualityScaling && !m_usingHighQualityScaling &&
        (Frame->m_rawWidth < width || Frame->m_rawHeight < height))
    {
        LOG(VB_GENERAL, LOG_INFO, "Enabling high quality scaling");
        m_usingHighQualityScaling = true;
    }
    else if (m_usingHighQualityScaling && !(Frame->m_rawWidth < displaysize.width() || Frame->m_rawHeight < displaysize.height()))
    {
        LOG(VB_GENERAL, LOG_INFO, "Disabling high quality scaling");
        m_usingHighQualityScaling = false;
    }

    return changed;
}

VideoRenderer* VideoRenderer::Create(VideoColourSpace *ColourSpace)
{
    VideoRenderer* render = NULL;

    RenderFactory* factory = RenderFactory::GetRenderFactory();
    for ( ; factory; factory = factory->NextFactory())
    {
        render = factory->Create(ColourSpace);
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
