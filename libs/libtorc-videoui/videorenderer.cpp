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
 * \todo Remove ColourPropertyAvailable/Unavailable signals (but need to prevent double signal to UI)
*/

VideoRenderer::VideoRenderer(VideoColourSpace *ColourSpace, UIWindow *Window)
  : QObject(),
    m_window(Window),
    m_outputFormat(AV_PIX_FMT_UYVY422),
    m_lastInputFormat(AV_PIX_FMT_NONE),
    m_validVideoFrame(false),
    m_lastFrameAspectRatio(1.77778f),
    m_lastFrameWidth(1920),
    m_lastFrameHeight(1080),
    m_lastFrameInverted(false),
    m_colourSpace(ColourSpace),
    m_updateFrameVertices(true),
    m_wantHighQualityScaling(false),
    m_usingHighQualityScaling(false),
    m_conversionContext(NULL)
{
    m_display = dynamic_cast<UIDisplay*>(Window);

    if (!m_display)
        LOG(VB_GENERAL, LOG_ERR, "Failed to find display object");

    m_wantHighQualityScaling = gLocalContext->GetPreference(TORC_VIDEO + "BicubicScaling", false);
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

QVariant VideoRenderer::GetProperty(TorcPlayer::PlayerProperty Property)
{
    switch (Property)
    {
        case TorcPlayer::HQScaling:
            return m_wantHighQualityScaling;
        default: break;
    }

    return QVariant();
}

bool VideoRenderer::SetProperty(TorcPlayer::PlayerProperty Property, QVariant Value)
{
    switch (Property)
    {
        case TorcPlayer::HQScaling:
        {
            m_wantHighQualityScaling = Value.toBool();
            gLocalContext->SetPreference(TORC_VIDEO + "BicubicScaling", m_wantHighQualityScaling);
            return true;
        }
        default: break;
    }

    return false;
}

bool VideoRenderer::DisplayReset(void)
{
    m_colourSpace->SetChanged(true);
    m_validVideoFrame = false;
    m_lastInputFormat = AV_PIX_FMT_NONE;
    return true;
}

QSet<TorcPlayer::PlayerProperty> VideoRenderer::GetSupportedProperties(void)
{
    return m_supportedProperties;
}

void VideoRenderer::ResetOutput(void)
{
    m_colourSpace->SetChanged(true);
    m_validVideoFrame         = false;
    m_usingHighQualityScaling = false;
    m_lastInputFormat         = AV_PIX_FMT_NONE;
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
        m_lastFrameInverted    = (Frame->m_invertForSource + Frame->m_invertForDisplay) & 1;
    }

    qreal avframePAR   = m_lastFrameAspectRatio;
    qreal displayPAR   = m_display->GetPixelAspectRatio();
    qreal width        = m_lastFrameWidth * avframePAR * displayPAR;
    qreal height       = m_lastFrameHeight;
    qreal widthfactor  = (qreal)Size.width() / width;
    qreal heightfactor = (qreal)Size.height() / height;
    bool invert        = m_lastFrameInverted;

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

    QRectF newrect(left, invert ? top + height : top, width, invert ? -height : height);
    bool changed = newrect != m_presentationRect;
    m_presentationRect = newrect;

    if (changed)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Display rect: %1x%2+%3+%4 (DAR %5)")
            .arg(width).arg(height).arg(left).arg(top).arg(displayPAR));
    }

    // enable/disable high quality scaling
    if (Frame && m_wantHighQualityScaling && !m_usingHighQualityScaling && (Frame->m_rawWidth < width || Frame->m_rawHeight < height) &&
        m_supportedProperties.contains(TorcPlayer::HQScaling))
    {
        LOG(VB_GENERAL, LOG_INFO, "Enabling high quality scaling");
        m_usingHighQualityScaling = true;
    }
    else if (Frame && m_usingHighQualityScaling &&
             (!(Frame->m_rawWidth < Size.width() || Frame->m_rawHeight < Size.height()) || !m_wantHighQualityScaling || !m_supportedProperties.contains(TorcPlayer::HQScaling)))
    {
        LOG(VB_GENERAL, LOG_INFO, "Disabling high quality scaling");
        m_usingHighQualityScaling = false;
    }

    return changed;
}

void VideoRenderer::UpdateSupportedProperties(const QSet<TorcPlayer::PlayerProperty> &Properties)
{
    QSet<TorcPlayer::PlayerProperty> removed = m_supportedProperties- Properties;
    QSet<TorcPlayer::PlayerProperty> added   = Properties - m_supportedProperties;

    foreach (TorcPlayer::PlayerProperty property, removed)
    {
        m_supportedProperties.remove(property);

        if (property == TorcPlayer::Brightness || property == TorcPlayer::Contrast ||
            property == TorcPlayer::Saturation || property == TorcPlayer::Hue)
        {
            emit ColourPropertyUnavailable(property);
        }
        else
        {
            emit PropertyUnavailable(property);
        }
    }

    foreach (TorcPlayer::PlayerProperty property, added)
    {
        m_supportedProperties.insert(property);

        if (property == TorcPlayer::Brightness || property == TorcPlayer::Contrast ||
            property == TorcPlayer::Saturation || property == TorcPlayer::Hue)
        {
            emit ColourPropertyAvailable(property);
        }
        else
        {
            emit PropertyAvailable(property);
        }
    }
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
