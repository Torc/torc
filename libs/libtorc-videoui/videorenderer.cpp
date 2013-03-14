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
#include "videoplayer.h"
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
    m_outputFormat(AV_PIX_FMT_UYVY422),
    m_lastInputFormat(AV_PIX_FMT_NONE),
    m_validVideoFrame(false),
    m_lastFrameAspectRatio(1.77778f),
    m_lastFrameWidth(1920),
    m_lastFrameHeight(1080),
    m_colourSpace(ColourSpace),
    m_updateFrameVertices(true),
    m_wantHighQualityScaling(false),
    m_allowHighQualityScaling(false),
    m_usingHighQualityScaling(false),
    m_conversionContext(NULL)
{
    m_display = dynamic_cast<UIDisplay*>(Window);

    if (!m_display)
        LOG(VB_GENERAL, LOG_ERR, "Failed to find display object");

    m_wantHighQualityScaling = gLocalContext->GetSetting(TORC_GUI + "BicubicScaling", false);
}

VideoRenderer::~VideoRenderer()
{
    sws_freeContext(m_conversionContext);
}

AVPixelFormat VideoRenderer::PreferredPixelFormat(void)
{
    return m_outputFormat;
}

void VideoRenderer::PlaybackFinished(void)
{
    ResetOutput();

    if (m_display)
        m_window->SetRefreshRate(m_display->GetDefaultRefreshRate(), m_display->GetDefaultMode());
}

bool VideoRenderer::HighQualityScalingAllowed(void)
{
    return m_allowHighQualityScaling;
}

bool VideoRenderer::HighQualityScalingEnabled(void)
{
    return m_usingHighQualityScaling;
}

bool VideoRenderer::GetHighQualityScaling(void)
{
    return m_wantHighQualityScaling;
}

bool VideoRenderer::SetHighQualityScaling(bool Enable)
{
    m_wantHighQualityScaling = Enable;
    gLocalContext->SetSetting(TORC_GUI + "BicubicScaling", m_wantHighQualityScaling);
    return true;
}

bool VideoRenderer::DisplayReset(void)
{
    m_colourSpace->SetChanged();
    m_validVideoFrame = false;
    return true;
}

void VideoRenderer::ResetOutput(void)
{
    m_colourSpace->SetChanged();
    m_validVideoFrame = false;
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

bool VideoRenderer::UpdatePosition(VideoFrame* Frame, const QSizeF &Size)
{
    if (!m_display)
        return false;

    if (Frame)
    {
        m_lastFrameAspectRatio = Frame->m_pixelAspectRatio;
        m_lastFrameWidth       = Frame->m_rawWidth;
        m_lastFrameHeight      = Frame->m_rawHeight;
    }

    qreal avframePAR   = m_lastFrameAspectRatio;
    qreal displayPAR   = m_display->GetPixelAspectRatio();
    qreal width        = m_lastFrameWidth * avframePAR * displayPAR;
    qreal height       = m_lastFrameHeight;
    qreal widthfactor  = (qreal)Size.width() / width;
    qreal heightfactor = (qreal)Size.height() / height;

    qreal left = 0.0f;
    qreal top  = 0.0f;

    if (widthfactor < heightfactor)
    {
        width *= widthfactor;
        height *= widthfactor;
        top = (Size.height() - height) / 2.0f;
    }
    else
    {
        width *= heightfactor;
        height *= heightfactor;
        left = (Size.width() - width) / 2.0f;
    }

    QRectF newrect(left, top, width, height);
    bool changed = newrect != m_presentationRect;
    m_presentationRect = newrect;

    if (changed)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Display rect: %1x%2+%3+%4 (DAR %5)")
            .arg(width).arg(height).arg(left).arg(top).arg(displayPAR));
    }

    // enable/disable high quality scaling
    if (m_wantHighQualityScaling && m_allowHighQualityScaling && !m_usingHighQualityScaling &&
        (Frame->m_rawWidth < width || Frame->m_rawHeight < height))
    {
        LOG(VB_GENERAL, LOG_INFO, "Enabling high quality scaling");
        m_usingHighQualityScaling = true;
    }
    else if (m_usingHighQualityScaling && (!(Frame->m_rawWidth < Size.width() || Frame->m_rawHeight < Size.height()) || !m_wantHighQualityScaling))
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
