/* Class VideoRendererDirect3D9
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2013
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
#include "uidirect3d9textures.h"
#include "uidirect3d9window.h"
#include "videocolourspace.h"
#include "videorenderdirect3d9.h"

static const char YUV2RGBVertexShader[] =
"struct Input\n"
"{\n"
"    float3 Position : POSITION0;\n"
"    float2 TexCoord : TEXCOORD0;\n"
"};\n"
"struct Output\n"
"{\n"
"    float4 Position : POSITION0;\n"
"    float2 TexCoord : TEXCOORD0;\n"
"};\n"
"\n"
"float4x4 Projection;\n"
"float4x4 Transform;\n"
"\n"
"Output main(Input In)\n"
"{\n"
"    Output Out;\n"
"    float4 world = mul(float4(In.Position, 1.0), Transform);\n"
"    Out.Position = mul(world, Projection);\n"
"    Out.TexCoord = In.TexCoord;\n"
"    return Out;\n"
"};\n";

static const char YUV2RGBFragmentShader[] =
"struct Output\n"
"{\n"
"    float4 Position : POSITION0;\n"
"    float2 TexCoord : TEXCOORD0;\n"
"};\n"
"sampler  Sampler;\n"
"row_major float4x4 ColorSpace;\n"
"float4 main(Output In): COLOR0\n"
"{\n"
"    float4 yuva = tex2D(Sampler, In.TexCoord);\n"
"    if (frac(In.TexCoord.x * SELECT_COLUMN) < 0.5)\n"
"        yuva = yuva.rabg;\n"
"    yuva = mul(float4(yuva.abr, 1.0), ColorSpace);\n"
"    return float4(yuva.rgb, 1.0);\n"
"}\n";

VideoRenderDirect3D9::VideoRenderDirect3D9(VideoColourSpace *ColourSpace, UIDirect3D9Window *Window)
  : VideoRenderer(ColourSpace, Window),
    m_direct3D9Window(Window),
    m_rawVideoTexture(NULL),
    m_rgbVideoTexture(NULL),
    m_yuvShader(NULL)
{
    m_allowHighQualityScaling = false;
}

VideoRenderDirect3D9::~VideoRenderDirect3D9()
{
    ResetOutput();
}

bool VideoRenderDirect3D9::DisplayReset(void)
{
    LOG(VB_GENERAL, LOG_INFO, "Resetting video rendering");

    if (m_direct3D9Window)
    {
        if (m_rawVideoTexture)
            m_direct3D9Window->DeleteTexture(m_rawVideoTexture);
        m_rawVideoTexture = NULL;

        if (m_rgbVideoTexture)
            m_direct3D9Window->DeleteTexture(m_rgbVideoTexture);
        m_rgbVideoTexture = NULL;
    }

    return VideoRenderer::DisplayReset();
}

void VideoRenderDirect3D9::ResetOutput(void)
{
    if (m_direct3D9Window)
    {
        if (m_rawVideoTexture)
            m_direct3D9Window->DeleteTexture(m_rawVideoTexture);
        m_rawVideoTexture = NULL;

        if (m_rgbVideoTexture)
            m_direct3D9Window->DeleteTexture(m_rgbVideoTexture);
        m_rgbVideoTexture = NULL;

        if (m_yuvShader)
            m_direct3D9Window->DeleteShader(m_yuvShader);
        m_yuvShader = NULL;
    }

    VideoRenderer::ResetOutput();
}

void VideoRenderDirect3D9::RefreshFrame(VideoFrame *Frame, const QSizeF &Size)
{
    if (!m_direct3D9Window)
        return;

    IDirect3DDevice9 *device = m_direct3D9Window->GetDevice();

    if (device && Frame && !Frame->m_corrupt)
    {
        // check for a size change
        if (m_rawVideoTexture)
        {
            if ((m_rawVideoTexture->m_usedSize.width() != (Frame->m_rawWidth / 2)) || (m_rawVideoTexture->m_usedSize.height() != Frame->m_rawHeight))
            {
                LOG(VB_GENERAL, LOG_INFO, QString("Video frame size changed from %1x%2 to %3x%4")
                    .arg(m_rawVideoTexture->m_usedSize.width() * 2).arg(m_rawVideoTexture->m_usedSize.height())
                    .arg(Frame->m_rawWidth).arg(Frame->m_rawHeight));
                ResetOutput();
            }
        }

        // create a texture if needed
        if (!m_rawVideoTexture)
        {
            QSize size(Frame->m_rawWidth / 2, Frame->m_rawHeight);
            m_rawVideoTexture = m_direct3D9Window->CreateTexture(device, size);

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
        // NB we could use yuv picture formats but we then have no control over
        // colourspace and adjustments
        if (!m_yuvShader)
        {
            unsigned int vertexsize   = sizeof(YUV2RGBVertexShader) / sizeof(char);
            unsigned int fragmentsize = sizeof(YUV2RGBFragmentShader) / sizeof(char);

            QByteArray fragment(YUV2RGBFragmentShader, fragmentsize);
            fragment.replace("SELECT_COLUMN", QByteArray::number(m_rawVideoTexture->m_size.width(), 'f', 8));

            m_yuvShader = m_direct3D9Window->CreateShader(device, YUV2RGBVertexShader, vertexsize,
                                                          fragment.data(), fragment.size());

            if (!m_yuvShader)
                return;
        }

        // create rgb texture
        if (!m_rgbVideoTexture)
        {
            QSize size(Frame->m_rawWidth, Frame->m_rawHeight);
            m_rgbVideoTexture = m_direct3D9Window->CreateTexture(device, size);

            if (!m_rgbVideoTexture)
            {
                LOG(VB_GENERAL, LOG_ERR, "Failed to create RGB video texture");
                return;
            }

            m_rgbVideoTexture->m_fullVertices = true;
        }

        // update the raw video texture
        D3DLOCKED_RECT d3drect;
        if (SUCCEEDED(m_rawVideoTexture->m_surface->LockRect(&d3drect, NULL, 0)))
        {
            void* buffer = d3drect.pBits;
            //if (d3drect.Pitch != m_rawVideoTexture->m_size.width() * 4)
            //    LOG(VB_GENERAL, LOG_WARNING, QString("Unexpected pitch %1 - expected %2").arg(d3drect.Pitch).arg(m_rawVideoTexture->m_size.width() * 4));

            PixelFormat informat = Frame->m_secondaryPixelFormat != PIX_FMT_NONE ? Frame->m_secondaryPixelFormat : Frame->m_pixelFormat;

            if (informat != m_outputFormat)
            {
                AVPicture in;
                avpicture_fill(&in, Frame->m_buffer, informat, Frame->m_adjustedWidth, Frame->m_adjustedHeight);
                AVPicture out;
                avpicture_fill(&out, (uint8_t*)buffer, m_outputFormat, d3drect.Pitch >> 1/*Frame->m_rawWidth*/, Frame->m_rawHeight);

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

            m_rawVideoTexture->m_surface->UnlockRect();

            // and upload to gpu
            IDirect3DSurface9 *surface;
            if (SUCCEEDED(m_rawVideoTexture->m_texture->GetSurfaceLevel(0, &surface)))
            {
                if (FAILED(device->StretchRect(m_rawVideoTexture->m_surface, NULL, surface, NULL, D3DTEXF_POINT)))
                    LOG(VB_GENERAL, LOG_ERR, "Failed to update video texture");
                else
                {
                    m_validVideoFrame = true;
                    surface->Release();
                }
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, "Failed to update video surface");
            }
        }

        // colour space conversion
        m_colourSpace->SetColourSpace(Frame->m_colourSpace);
        m_colourSpace->SetStudioLevels(m_window->GetStudioLevels());
        if (m_colourSpace->HasChanged())
        {
            D3DXMATRIX matrix;
            memcpy(&matrix, m_colourSpace->Data(), sizeof(float) * 16);
            if (FAILED(m_yuvShader->m_pixelConstants->SetMatrixTranspose(device, "ColorSpace", &matrix)))
                LOG(VB_GENERAL, LOG_INFO, "Failed to update colorspace matrix");
        }

        QRect viewport = m_direct3D9Window->GetViewPort();
        QRect view(QPoint(0, 0), m_rgbVideoTexture->m_usedSize);
        QRectF destination(0.0, 0.0, m_rgbVideoTexture->m_usedSize.width(),m_rgbVideoTexture->m_usedSize.height());
        QSizeF size(m_rawVideoTexture->m_usedSize);

        m_direct3D9Window->SetBlend(false);
        m_direct3D9Window->SetRenderTarget(m_rgbVideoTexture);
        m_direct3D9Window->SetViewPort(device, view, UIDirect3D9View::Parallel);
        bool dummy = false;
        m_direct3D9Window->DrawTexture(m_yuvShader, m_rawVideoTexture, &destination, &size, dummy);
        m_direct3D9Window->SetRenderTarget(NULL);
        m_direct3D9Window->SetViewPort(device, viewport, UIDirect3D9View::Parallel);

        // update the display setup
        UpdateRefreshRate(Frame);
    }

    m_updateFrameVertices |= UpdatePosition(Frame, Size);
}

void VideoRenderDirect3D9::RenderFrame(void)
{
    if (m_validVideoFrame && m_direct3D9Window && m_rgbVideoTexture)
    {
        QSizeF size = m_rgbVideoTexture->m_usedSize;
        m_direct3D9Window->DrawTexture(m_rgbVideoTexture, &m_presentationRect, &size, m_updateFrameVertices);
    }
}

class RenderDirect3D9Factory : public RenderFactory
{
    VideoRenderer* Create(VideoColourSpace *ColourSpace)
    {
        UIDirect3D9Window* window = static_cast<UIDirect3D9Window*>(gLocalContext->GetUIObject());
        if (window)
            return new VideoRenderDirect3D9(ColourSpace, window);

        return NULL;
    }
} RenderDirect3D9Factory;

