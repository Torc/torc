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
#include "videocolourspace.h"
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

static const char DefaultFragmentShader[] =
"GLSL_DEFINES"
"RGB_DEFINES"
"uniform RGB_SAMPLER s_texture0;\n"
"varying vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    gl_FragColor = RGB_TEXTURE(s_texture0, v_texcoord0);\n"
"}\n";

static const char BicubicShader[] =
"GLSL_DEFINES"
"RGB_DEFINES"
"uniform sampler2DRect s_texture0;\n"
"varying vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    vec2 coord = v_texcoord0 - vec2(0.5, 0.5);\n"
"    vec2 index = floor(coord);\n"
"    vec2 fract = coord - index;\n"
"    vec2 one   = 1.0 - fract;\n"
"    vec2 two   = one * one;\n"
"    vec2 frac2 = fract * fract;\n"
"    vec2 w0    = (1.0 / 6.0) * (two * one);\n"
"    vec2 w1    = (2.0 / 3.0) - ((0.5 * frac2) * (2.0 - fract));\n"
"    vec2 w2    = (2.0 / 3.0) - ((0.5 * two) * (2.0 - one));\n"
"    vec2 w3    = (1.0 / 6.0) * (frac2 * fract);\n"
"    vec2 g0    = w0 + w1;\n"
"    vec2 g1    = w2 + w3;\n"
"    vec2 h0    = (w1 / g0) - 0.5 + index;\n"
"    vec2 h1    = (w3 / g1) + 1.5 + index;\n"
"    vec4 tex00 = texture2DRect(s_texture0, h0);\n"
"    vec4 tex10 = texture2DRect(s_texture0, vec2(h1.x, h0.y));\n"
"    vec4 tex01 = texture2DRect(s_texture0, vec2(h0.x, h1.y));\n"
"    vec4 tex11 = texture2DRect(s_texture0, h1);\n"
"    tex00      = mix(tex01, tex00, g0.y);\n"
"    tex10      = mix(tex11, tex10, g0.y);\n"
"    gl_FragColor = mix(tex10, tex00, g0.x);\n"
"}\n";

VideoRendererOpenGL::VideoRendererOpenGL(VideoColourSpace *ColourSpace, UIOpenGLWindow *Window)
  : VideoRenderer(ColourSpace, Window),
    m_openglWindow(Window),
    m_outputFormat(PIX_FMT_UYVY422),
    m_validVideoFrame(false),
    m_updateFrameVertices(true),
    m_rawVideoTexture(NULL),
    m_rgbVideoTexture(NULL),
    m_rgbVideoBuffer(0),
    m_yuvShader(0),
    m_rgbShader(0),
    m_bicubicShader(0),
    m_conversionContext(NULL)
{
    m_allowHighQualityScaling = m_openglWindow->IsRectTexture(m_openglWindow->GetRectTextureType());
}

VideoRendererOpenGL::~VideoRendererOpenGL()
{
    ResetOutput();
    sws_freeContext(m_conversionContext);
}

void VideoRendererOpenGL::ResetOutput(void)
{
    if (m_rawVideoTexture)
        m_openglWindow->DeleteTexture(m_rawVideoTexture->m_val);
    m_rawVideoTexture = NULL;

    if (m_rgbVideoBuffer)
        m_openglWindow->DeleteFrameBuffer(m_rgbVideoBuffer);
    m_rgbVideoBuffer = 0;

    if (m_rgbVideoTexture)
        m_openglWindow->DeleteTexture(m_rgbVideoTexture->m_val);
    m_rgbVideoTexture = NULL;

    m_openglWindow->DeleteShaderObject(m_yuvShader);
    m_yuvShader = 0;

    m_openglWindow->DeleteShaderObject(m_rgbShader);
    m_rgbShader = 0;

    m_openglWindow->DeleteShaderObject(m_bicubicShader);
    m_bicubicShader = 0;

    m_colourSpace->SetChanged();

    m_validVideoFrame = false;

    VideoRenderer::ResetOutput();
}

void VideoRendererOpenGL::RefreshFrame(VideoFrame *Frame, const QSizeF &Size)
{
    if (!m_openglWindow)
        return;

    if (Frame && !Frame->m_corrupt)
    {
        // check for a size change
        if (m_rawVideoTexture)
        {
            if ((m_rawVideoTexture->m_actualSize.width() != (Frame->m_rawWidth / 2)) || (m_rawVideoTexture->m_actualSize.height() != Frame->m_rawHeight))
            {
                LOG(VB_GENERAL, LOG_INFO, QString("Video frame size changed from %1x%2 to %3x%4")
                    .arg(m_rawVideoTexture->m_actualSize.width() * 2).arg(m_rawVideoTexture->m_actualSize.height())
                    .arg(Frame->m_rawWidth).arg(Frame->m_rawHeight));
                ResetOutput();
            }
        }

        // create a texture if needed
        if (!m_rawVideoTexture)
        {
            QSize size(Frame->m_rawWidth / 2, Frame->m_rawHeight);
            m_rawVideoTexture = m_openglWindow->CreateTexture(size, true, 0, GL_UNSIGNED_BYTE, GL_RGBA, GL_RGBA, GL_NEAREST);

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

        // create a yuv to rgb shader
        if (!m_yuvShader)
        {
            QByteArray vertex(YUV2RGBVertexShader);
            QByteArray fragment(YUV2RGBFragmentShader);
            CustomiseShader(fragment);
            m_yuvShader = m_openglWindow->CreateShaderObject(vertex, fragment);

            if (!m_yuvShader)
                return;
        }

        // TODO check for FBO support
        // create rgb texture and framebuffer
        if (!m_rgbVideoTexture)
        {
            QSize size(Frame->m_rawWidth, Frame->m_rawHeight);
            m_rgbVideoTexture = m_openglWindow->CreateTexture(size, false, m_allowHighQualityScaling ? m_openglWindow->GetRectTextureType() : 0,
                                                              GL_UNSIGNED_BYTE, GL_RGBA, GL_RGBA);

            if (!m_rgbVideoTexture)
            {
                LOG(VB_GENERAL, LOG_ERR, "Failed to create RGB video texture");
                return;
            }

            m_rgbVideoTexture->m_fullVertices = true;
        }

        // create a plain rgb shader. We need to use a custom shader in case
        // the texture type differs from the main UI type (bicubic requires rect)
        if (!m_rgbShader)
        {
            QByteArray fragment(DefaultFragmentShader);
            CustomiseShader(fragment);
            m_rgbShader = m_openglWindow->CreateShaderObject(QByteArray(), fragment);

            if (!m_rgbShader)
                return;
        }

        if (!m_rgbVideoBuffer && m_rgbVideoTexture)
        {
            if (!m_openglWindow->CreateFrameBuffer(m_rgbVideoBuffer, m_rgbVideoTexture->m_val))
            {
                LOG(VB_GENERAL, LOG_ERR, "Failed to create RGB video frame buffer");
                return;
            }
        }

        // update the textures
        void* buffer = m_openglWindow->GetTextureBuffer(m_rawVideoTexture);

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

        m_openglWindow->UpdateTexture(m_rawVideoTexture, buffer);
        m_validVideoFrame = true;

        // colour space conversion
        QRect viewport = m_openglWindow->GetViewPort();
        QRect view(QPoint(0, 0), m_rgbVideoTexture->m_actualSize);
        QRectF destination(QPointF(0.0, m_rgbVideoTexture->m_actualSize.height()),
                           QPointF(m_rgbVideoTexture->m_actualSize.width(), 0.0));
        QSizeF size(m_rawVideoTexture->m_actualSize);

        m_openglWindow->SetBlend(false);
        m_openglWindow->BindFramebuffer(m_rgbVideoBuffer);
        m_openglWindow->SetViewPort(view);
        m_colourSpace->SetColourSpace(Frame->m_colourSpace);
        m_colourSpace->SetStudioLevels(m_window->GetStudioLevels());
        if (m_colourSpace->HasChanged())
            m_openglWindow->SetShaderParams(m_yuvShader, m_colourSpace->Data(), "COLOUR_UNIFORM");
        m_openglWindow->DrawTexture(m_rawVideoTexture, &destination, &size, m_yuvShader);
        m_openglWindow->SetViewPort(viewport);

        // update the display setup
        UpdateRefreshRate(Frame);
    }

    // update position even with no frame as the display may be animated while paused/seeking etc
    m_updateFrameVertices |= UpdatePosition(Frame, Size);
}

void VideoRendererOpenGL::RenderFrame(void)
{
    if (m_validVideoFrame && m_openglWindow && m_rgbVideoTexture)
    {
        QSizeF size = m_rgbVideoTexture->m_actualSize;
        m_openglWindow->SetBlend(false);
        m_openglWindow->BindFramebuffer(0);

        if (m_updateFrameVertices)
        {
            m_rgbVideoTexture->m_vboUpdated = false;
            m_updateFrameVertices = false;
        }

        if (m_usingHighQualityScaling)
        {
            if (!m_bicubicShader)
            {
                QByteArray bicubic(BicubicShader);
                CustomiseShader(bicubic);
                m_bicubicShader = m_openglWindow->CreateShaderObject(QByteArray(), bicubic);
            }

            m_openglWindow->DrawTexture(m_rgbVideoTexture, &m_presentationRect, &size, m_bicubicShader);
        }
        else
        {
            m_openglWindow->DrawTexture(m_rgbVideoTexture, &m_presentationRect, &size, m_rgbShader);
        }
    }
}

PixelFormat VideoRendererOpenGL::PreferredPixelFormat(void)
{
    return m_outputFormat;
}

void VideoRendererOpenGL::CustomiseShader(QByteArray &Source)
{
    float selectcolumn = 1.0f;

    if (m_rawVideoTexture && !m_openglWindow->IsRectTexture(m_rawVideoTexture->m_type) && m_rawVideoTexture->m_size.width() > 0)
        selectcolumn /= ((float)m_rawVideoTexture->m_size.width());

    QByteArray extensions = "";
    QByteArray rgbsampler = "sampler2D";
    QByteArray rgbtexture = "texture2D";

    if (m_allowHighQualityScaling)
    {
        extensions += "#extension GL_ARB_texture_rectangle : enable\n";
        rgbsampler += "Rect";
        rgbtexture += "Rect";
    }

    Source.replace("RGB_SAMPLER", rgbsampler);
    Source.replace("RGB_TEXTURE", rgbtexture);
    Source.replace("RGB_DEFINES", extensions);

    Source.replace("SELECT_COLUMN", QByteArray::number(1.0f / selectcolumn, 'f', 8));
}

class RenderOpenGLFactory : public RenderFactory
{
    VideoRenderer* Create(VideoColourSpace *ColourSpace)
    {
        UIOpenGLWindow* window = static_cast<UIOpenGLWindow*>(gLocalContext->GetUIObject());
        if (window)
            return new VideoRendererOpenGL(ColourSpace, window);

        return NULL;
    }
} RenderOpenGLFactory;
