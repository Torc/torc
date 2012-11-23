/* Class VideoRendererOpenGL
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
#include "uiopengltextures.h"
#include "uiopenglwindow.h"
#include "videorendereropengl.h"

static const char YUV2RGBVertexShader[] =
"GLSL_DEFINES"
"attribute vec3 a_position;\n"
"attribute vec2 a_texcoord0;\n"
"varying   vec2 v_texcoord0;\n"
"uniform   mat4 u_projection;\n"
"uniform   mat4 u_transform;\n"
"void main() {\n"
"    gl_Position = u_projection * u_transform * vec4(a_position, 1.0);\n"
"    v_texcoord0 = a_texcoord0;\n"
"}\n";

static const char YUV2RGBFragmentShader[] =
"GLSL_DEFINES"
"uniform GLSL_SAMPLER s_texture0;\n"
"uniform mat4 COLOUR_UNIFORM;\n"
"varying vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    vec4 yuva = GLSL_TEXTURE(s_texture0, v_texcoord0);\n"
"    if (fract(v_texcoord0.x * SELECT_COLUMN) < 0.5)\n"
"        yuva = yuva.rabg;\n"
"    gl_FragColor = vec4(yuva.arb, 1.0) * COLOUR_UNIFORM;\n"
"}\n";

VideoRendererOpenGL::VideoRendererOpenGL(UIOpenGLWindow *Window)
  : VideoRenderer(Window),
    m_window(Window),
    m_outputFormat(PIX_FMT_UYVY422),
    m_validVideoFrame(false),
    m_rawVideoTexture(NULL),
    m_rgbVideoTexture(NULL),
    m_rgbVideoBuffer(0),
    m_videoShader(0),
    m_colourSpace(new VideoColourSpace(AVCOL_SPC_UNSPECIFIED)),
    m_conversionContext(NULL)
{
}

VideoRendererOpenGL::~VideoRendererOpenGL()
{
    ResetOutput();
    delete m_colourSpace;
    sws_freeContext(m_conversionContext);
}

void VideoRendererOpenGL::ResetOutput(void)
{
    if (m_rawVideoTexture)
        m_window->DeleteTexture(m_rawVideoTexture->m_val);
    m_rawVideoTexture = NULL;

    if (m_rgbVideoBuffer)
        m_window->DeleteFrameBuffer(m_rgbVideoBuffer);
    m_rgbVideoBuffer = 0;

    if (m_rgbVideoTexture)
        m_window->DeleteTexture(m_rgbVideoTexture->m_val);
    m_rgbVideoTexture = NULL;

    m_window->DeleteShaderObject(m_videoShader);
    m_videoShader = 0;

    m_validVideoFrame = false;
}

void VideoRendererOpenGL::RenderFrame(VideoFrame *Frame)
{
    if (!m_window)
        return;

    bool updatevertices = false;

    if (Frame)
    {
        // check for a size change
        if (m_rawVideoTexture)
        {
            if ((m_rawVideoTexture->m_actualSize.width() != (Frame->m_rawWidth / 2)) || (m_rawVideoTexture->m_actualSize.height() != Frame->m_rawHeight))
            {
                LOG(VB_GENERAL, LOG_INFO, QString("Video frame size changed from %1x%2 to %3x%4")
                    .arg(m_rawVideoTexture->m_actualSize.width()).arg(m_rawVideoTexture->m_actualSize.height())
                    .arg(Frame->m_rawWidth).arg(Frame->m_rawHeight));
                ResetOutput();
            }
        }

        // create a texture if needed
        if (!m_rawVideoTexture)
        {
            QSize size(Frame->m_rawWidth / 2, Frame->m_rawHeight);
            m_rawVideoTexture = m_window->CreateTexture(size, true, 0, GL_UNSIGNED_BYTE, GL_RGBA, GL_RGBA, GL_NEAREST);

            if (!m_rawVideoTexture)
            {
                LOG(VB_GENERAL, LOG_ERR, "Failed to create video texture");
                return;
            }

            // standard texture vertices only use width and height, we need the offset
            // as well to ensure texture inversion works
            m_rawVideoTexture->m_fullVertices = true;

            LOG(VB_GENERAL, LOG_INFO, QString("Created video texture %1x%2").arg(Frame->m_rawWidth).arg(Frame->m_rawHeight));
        }

        // create a shader
        if (!m_videoShader)
        {
            QByteArray vertex(YUV2RGBVertexShader);
            QByteArray fragment(YUV2RGBFragmentShader);
            CustomiseShader(fragment);
            m_videoShader = m_window->CreateShaderObject(vertex, fragment);

            if (!m_videoShader)
                return;
        }

        // TODO optimise this away when not needed
        // TODO check for FBO support
        // create rgb texture and framebuffer
        if (!m_rgbVideoTexture)
        {
            QSize size(Frame->m_rawWidth, Frame->m_rawHeight);
            m_rgbVideoTexture = m_window->CreateTexture(size, false, 0, GL_UNSIGNED_BYTE, GL_RGBA, GL_RGBA);

            if (!m_rgbVideoTexture)
            {
                LOG(VB_GENERAL, LOG_ERR, "Failed to create RGB video texture");
                return;
            }

            m_rgbVideoTexture->m_fullVertices = true;
        }

        if (!m_rgbVideoBuffer && m_rgbVideoTexture)
        {
            if (!m_window->CreateFrameBuffer(m_rgbVideoBuffer, m_rgbVideoTexture->m_val))
            {
                LOG(VB_GENERAL, LOG_ERR, "Failed to create RGB video frame buffer");
                return;
            }
        }

        // update the textures
        void* buffer = m_window->GetTextureBuffer(m_rawVideoTexture);

        PixelFormat informat  = Frame->m_secondaryPixelFormat != PIX_FMT_NONE ? Frame->m_secondaryPixelFormat : Frame->m_pixelFormat;

        if (informat != m_outputFormat)
        {
            AVPicture in;
            avpicture_fill(&in, Frame->m_buffer, informat, Frame->m_adjustedWidth, Frame->m_adjustedHeight);
            AVPicture out;
            avpicture_fill(&out, (uint8_t*)buffer, m_outputFormat, Frame->m_rawWidth, Frame->m_rawHeight);

            m_conversionContext = sws_getCachedContext(m_conversionContext,
                                                       Frame->m_rawWidth, Frame->m_rawHeight, informat,
                                                       Frame->m_rawWidth, Frame->m_rawHeight, m_outputFormat,
                                                       SWS_FAST_BILINEAR, NULL, NULL, NULL);
            if (m_conversionContext != NULL)
            {
                if (sws_scale(m_conversionContext, in.data, in.linesize, 0, Frame->m_rawHeight, out.data, out.linesize) < 1)
                    LOG(VB_GENERAL, LOG_ERR, "Software scaling/conversion failed");
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, "Failed to create software conversion context");
            }
        }
        else
        {
            memcpy(buffer, Frame->m_buffer, Frame->m_bufferSize);
        }

        m_window->UpdateTexture(m_rawVideoTexture, buffer);
        m_validVideoFrame = true;

        // colour space conversion
        QRect viewport = m_window->GetViewPort();
        QRect view(QPoint(0, 0), m_rgbVideoTexture->m_actualSize);
        QRectF destination(QPointF(0.0, m_rgbVideoTexture->m_actualSize.height()),
                           QPointF(m_rgbVideoTexture->m_actualSize.width(), 0.0));
        QSizeF size(m_rawVideoTexture->m_actualSize);

        m_window->SetBlend(false);
        m_window->BindFramebuffer(m_rgbVideoBuffer);
        m_window->SetViewPort(view);
        m_colourSpace->SetColourSpace(Frame->m_colourSpace);
        if (m_colourSpace->HasChanged())
            m_window->SetShaderParams(m_videoShader, m_colourSpace->Data(), "COLOUR_UNIFORM");
        m_window->DrawTexture(m_rawVideoTexture, &destination, &size, m_videoShader);
        m_window->SetViewPort(viewport);

        // update the display setup
        updatevertices = UpdatePosition(Frame);
    }

    if (m_validVideoFrame)
    {
        // and finally draw
        QSizeF size = m_rgbVideoTexture->m_actualSize;
        m_window->DrawTexture(m_rgbVideoTexture, &m_presentationRect, &size, updatevertices, false);
    }
}

PixelFormat VideoRendererOpenGL::PreferredPixelFormat(void)
{
    return m_outputFormat;
}

void VideoRendererOpenGL::CustomiseShader(QByteArray &Source)
{
    float selectcolumn = 1.0f;

    if (m_rawVideoTexture && !m_window->IsRectTexture(m_rawVideoTexture->m_type && m_rawVideoTexture->m_size.width() > 0))
        selectcolumn /= ((float)m_rawVideoTexture->m_size.width());

    Source.replace("SELECT_COLUMN", QByteArray::number(1.0f / selectcolumn, 'f', 8));
}

class RenderOpenGLFactory : public RenderFactory
{
    VideoRenderer* Create(void)
    {
        UIOpenGLWindow* window = static_cast<UIOpenGLWindow*>(gLocalContext->GetUIObject());
        if (window)
            return new VideoRendererOpenGL(window);

        return NULL;
    }
} RenderOpenGLFactory;
