/* Class TorcSGVideoProvider
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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

// Qt
#include <QtGui/QOpenGLFunctions>

// Torc
#include "torclogging.h"
#include "torcavutils.h"
#include "videoframe.h"
#include "videocolourspace.h"
#include "videodecoder.h"
#include "torcsgvideoprovider.h"

inline uint OpenGLBufferSize(QSize Size, GLuint DataFormat, GLuint DataType)
{
    uint bytes;
    uint bpp;

    if (DataFormat == GL_BGRA || DataFormat == GL_RGBA || DataFormat == GL_RGBA8)
    {
        bpp = 4;
    }
    else if (DataFormat == GL_YCBCR_MESA || DataFormat == GL_YCBCR_422_APPLE || DataFormat == TORC_UYVY)
    {
        bpp = 2;
    }
    else
    {
        LOG(VB_GENERAL, LOG_WARNING, "Unknown OpenGL data format");
        bpp = 0;
    }

    switch (DataType)
    {
        case GL_UNSIGNED_BYTE:
            bytes = sizeof(GLubyte);
            break;
        case GL_UNSIGNED_SHORT_8_8_MESA:
            bytes = sizeof(GLushort);
            break;
        case GL_FLOAT:
            bytes = sizeof(GLfloat);
            break;
        default:
            LOG(VB_GENERAL, LOG_WARNING, "Unknown OpenGL data format");
            bytes = 0;
    }

    if (!bpp || !bytes || Size.width() < 1 || Size.height() < 1)
        return 0;

    return Size.width() * Size.height() * bpp * bytes;

}

inline QSize OpenGLTextureSize(const QSize &Size, bool Rectangular)
{
    if (Rectangular)
        return Size;

    int width  = 64;
    int height = 64;

    while (width < Size.width())
        width *= 2;

    while (height < Size.height())
        height *= 2;

    return QSize(width, height);
}

/*! \class TorcSGVideoProvider
 *  \brief A class to provide and update a video texture.
 *
 * TorcSGVideoProvider takes a video frame as input (hardware accelerated and software decoded) and
 * makes it available as an OpenGL texture for presentation and manipulation.
 *
 * \note As the 'SG' suggests, use this class like any other scene graph object i.e. only from within updatePaintNode.
 *
 * \note Rectangular textures are supported for the RGB framebuffer (they are not needed for the raw input frame) but
 *       additional support is required at the QML level to allow their use (Qt only uses 2D textures in there shaders).
 *
 * \todo Add proper failure mode or fallback for lack of FramebufferObject support.
 * \todo Add pixel buffer object support.
 * \todo Add bicubic scaling support.
 * \todo Add proper video scaling/position.
 * \todo Refactor hardware AccelerationFactory to support new code.
*/
TorcSGVideoProvider::TorcSGVideoProvider(VideoColourSpace *ColourSpace)
  : QSGTexture(),
    QSGTextureProvider(),
    m_rawVideoTexture(0),
    m_rawVideoTextureSize(QSize(0,0)),
    m_rawVideoTextureSizeUsed(QSize(0,0)),
    m_YUV2RGBShader(NULL),
    m_YUV2RGBShaderColourLocation(-1),
    m_rgbVideoFrameBuffer(NULL),
    m_rgbVideoTextureType(GL_TEXTURE_2D),
    m_rgbVideoTextureSize(QSize(0,0)),
    m_rgbVideoTextureSizeUsed(QSize(0,0)),
    m_openGLContext(NULL),
    m_haveNPOTTextures(false),
    m_useNPOTTextures(false),
    m_haveRectangularTextures(false),
    m_useRectangularTextures(false),
    m_needRectangularTextures(false),
    m_haveFramebuffers(false),
    m_ignoreCorruptFrames(true),
    m_corruptFrameCount(0),
    m_outputFormat(AV_PIX_FMT_UYVY422),
    m_lastInputFormat(AV_PIX_FMT_NONE),
    m_validVideoFrame(false),
    m_lastFrameAspectRatio(1.77778f),
    m_lastFrameWidth(1920),
    m_lastFrameHeight(1080),
    m_lastFrameInverted(false),
    m_dirtyGeometry(true),
    m_videoColourSpace(ColourSpace),
    m_conversionContext(NULL)
{
    m_openGLContext = QOpenGLContext::currentContext();

    if (!m_openGLContext)
    {
        LOG(VB_GENERAL, LOG_ERR, "No OpenGL context");
    }
    else
    {
        // check OpenGL feature support
        if (m_openGLContext->functions()->hasOpenGLFeature(QOpenGLFunctions::NPOTTextures))
        {
            LOG(VB_GENERAL, LOG_INFO, "NPOT textures available");
            m_haveNPOTTextures = true;
        }

        if (m_openGLContext->hasExtension("GL_NV_texture_rectangle") ||
            m_openGLContext->hasExtension("GL_ARB_texture_rectangle") ||
            m_openGLContext->hasExtension("GL_EXT_texture_rectangle"))
        {
            LOG(VB_GENERAL, LOG_INFO, "Rectangular textures available");
            m_haveRectangularTextures = true;
        }

        if (m_openGLContext->functions()->hasOpenGLFeature(QOpenGLFunctions::Framebuffers))
        {
            LOG(VB_GENERAL, LOG_INFO, "OpenGL framebuffers available");
            m_haveFramebuffers = true;
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING, "No OpenGL framebuffer support available");
        }

        if (m_openGLContext->format().renderableType() == QSurfaceFormat::OpenGLES)
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Using OpenGL ES %1.%2")
                .arg(m_openGLContext->format().majorVersion())
                .arg(m_openGLContext->format().minorVersion()));
            m_GLSLVersion = "#version 100\n";
            m_qualifiers  = "precision mediump float;\n";
        }
        else if (m_openGLContext->format().renderableType() == QSurfaceFormat::OpenGL)
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Using OpenGL %1.%2")
                .arg(m_openGLContext->format().majorVersion())
                .arg(m_openGLContext->format().minorVersion()));
            m_GLSLVersion = "#version 110\n";
            m_qualifiers  = QByteArray();
        }

        // create a default framebuffer now to avoid on screen garbage
        m_rgbVideoFrameBuffer = new QOpenGLFramebufferObject(QSize(128,128), m_rgbVideoTextureType);
        if (m_rgbVideoFrameBuffer->isValid())
        {
            m_rgbVideoFrameBuffer->bind();
            glClear(GL_COLOR_BUFFER_BIT);
            m_rgbVideoFrameBuffer->release();
        }

        CustomiseTextures();
    }
}

TorcSGVideoProvider::~TorcSGVideoProvider()
{
    sws_freeContext(m_conversionContext);
    Reset();
}

/// Returns the QSGTexture associated with this object.
QSGTexture* TorcSGVideoProvider::texture(void) const
{
    return (QSGTexture*)this;
}

/// Returns the OpenGL texture handle for the converted video frame.
int TorcSGVideoProvider::textureId(void) const
{
    if (m_rgbVideoFrameBuffer)
        return (int)m_rgbVideoFrameBuffer->texture();

    return 0;
}

/// Returns the size of the OpenGL video frame.
QSize TorcSGVideoProvider::textureSize(void) const
{
    return m_rgbVideoTextureSize;
}

bool TorcSGVideoProvider::hasAlphaChannel(void) const
{
    return false;
}

bool TorcSGVideoProvider::hasMipmaps(void) const
{
    return false;
}

/// Binds the converted OpenGL video frame for texture operations.
void TorcSGVideoProvider::bind(void)
{
    if (m_rgbVideoFrameBuffer)
        glBindTexture(m_rgbVideoTextureType, m_rgbVideoFrameBuffer->texture());
}

/*! \brief Resets all video state.
 *
 * This is usually triggered when the video format and/or size changes.
*/
void TorcSGVideoProvider::Reset(void)
{
    m_ignoreCorruptFrames = true;
    m_corruptFrameCount = 0;

    m_videoColourSpace->SetChanged(true);
    m_validVideoFrame = false;
    m_lastInputFormat = AV_PIX_FMT_NONE;

    if (m_rgbVideoFrameBuffer)
        delete m_rgbVideoFrameBuffer;
    m_rgbVideoFrameBuffer = NULL;
    m_rgbVideoTextureType = GL_TEXTURE_2D;
    m_rgbVideoTextureSize = QSize(0, 0);
    m_rgbVideoTextureSizeUsed = QSize(0, 0);

    if (m_rawVideoTexture)
        glDeleteTextures(1, &m_rawVideoTexture);
    m_rawVideoTexture = 0;
    m_rawVideoTextureSize = QSize(0, 0);
    m_rawVideoTextureSizeUsed = QSize(0, 0);

    if (m_YUV2RGBShader)
        delete m_YUV2RGBShader;
    m_YUV2RGBShader = NULL;
    m_YUV2RGBShaderColourLocation = -1;

    m_conversionBuffer.resize(0);

    CustomiseTextures();
}

///\brief Inform the parent that the video geometry needs updating
bool TorcSGVideoProvider::GetDirtyGeometry(void)
{
    if (m_dirtyGeometry)
    {
        m_dirtyGeometry = false;
        return true;
    }

    return false;
}

/*! \brief Provide the subrect of ParentGeometry in which the current video frame should be displayed.
 *
 * /todo Check using non-square display PAR.
*/
QRectF TorcSGVideoProvider::GetGeometry(const QRectF &ParentGeometry, qreal DisplayAspectRatio)
{
    qreal width        = m_lastFrameHeight * m_lastFrameAspectRatio;
    qreal height       = m_lastFrameHeight;
    qreal widthfactor  = (qreal)ParentGeometry.width() / width;
    qreal heightfactor = (qreal)ParentGeometry.height() / height;

    qreal left = 0.0f;
    qreal top  = 0.0f;

    if (widthfactor < heightfactor)
    {
        width *= widthfactor;
        height *= widthfactor;
        top = (ParentGeometry.height() - height) / 2.0f;
    }
    else
    {
        width *= heightfactor;
        height *= heightfactor;
        left = (ParentGeometry.width() - width) / 2.0f;
    }

    return QRectF(left , m_lastFrameInverted ? top + height : top, width, m_lastFrameInverted ? -height : height);
}

/*! \brief Setup specific texture requirements.
 *
 * This is used to setup rectangular textures if they are available and required. Otherwise
 * 'non-power-of-two' textures are used if available (to save video memory), with a fallback to normal 2D textures.
 *
 * Rectangular textures use 'regular' (i.e. non-normalised) texture co-ordinates, which allows
 * optimisations for certain shader operations (e.g. bicubic scaling).
 */
void TorcSGVideoProvider::CustomiseTextures(void)
{
    // defaults
    m_rgbVideoTextureTypeDefault = GL_TEXTURE_2D;
    m_useRectangularTextures     = false;
    m_useNPOTTextures            = false;

    // decide on texturing support
    if (m_needRectangularTextures && m_haveRectangularTextures)
    {
        LOG(VB_GENERAL, LOG_INFO, "Using rectangular textures.");
        m_rgbVideoTextureTypeDefault = GL_TEXTURE_RECTANGLE_ARB;
        m_useRectangularTextures = true;
    }
    else if (m_haveNPOTTextures)
    {
        LOG(VB_GENERAL, LOG_INFO, "Using NPOT textures");
        m_useNPOTTextures = true;
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, "Using plain 2D textures");
    }
}

/*! \brief Customise shader source for Torc specific requirements
 *
 * \todo Review what is still needed here.
*/
void TorcSGVideoProvider::CustomiseShader(QByteArray &Source, GLenum TextureType, QSize &Size, QSize &UsedSize)
{
    Q_UNUSED(TextureType);

    bool  rectangle    = false;
    float selectcolumn = 1.0f;
    float halfwidth    = Size.width() / 2.0f;
    float halfheight   = Size.height() / 2.0f;

    QByteArray extensions = "";
    QByteArray sampler    = "sampler2D";
    QByteArray texture    = "texture2D";

    if (rectangle)
    {
        extensions += "#extension GL_ARB_texture_rectangle : enable\n";
        sampler    += "Rect";
        texture    += "Rect";
    }
    else if (UsedSize.width() > 0)
    {
        selectcolumn /= ((float)UsedSize.width());
        halfwidth    /= ((float)UsedSize.width());
        halfheight   /= ((float)UsedSize.height());
    }

    Source.replace("GLSL_SAMPLER",  sampler);
    Source.replace("GLSL_TEXTURE",  texture);
    Source.replace("GLSL_DEFINES",  m_GLSLVersion + extensions + m_qualifiers);
    Source.replace("SELECT_COLUMN", QByteArray::number(1.0f / selectcolumn, 'f', 8));
    Source.replace("HALF_WIDTH",    QByteArray::number(halfwidth, 'f', 8));
    Source.replace("HALF_HEIGHT",   QByteArray::number(halfheight, 'f', 8));
}

/*! \brief Updates the OpenGL video texture with the contents of the VideoFrame.
 *
 * This method is the guts of the class.
 *
 * For software based decoding, the contents of VideoFrame are converted to a packed YUV format,
 * uploaded to a 'raw' OpenGL texture and then rendered into a FramebufferObject using OpenGL
 * shaders to perform the colourspace conversion and colourspace adjustments.
 *
 * For hardware decoding, the AccelerationFactory subclass is responsible for copying the
 * decoded frame directly to the FramebufferObject.
 *
 * \todo Refactor to remove code duplication and break up into more manageable functions.
*/
bool TorcSGVideoProvider::Refresh(VideoFrame *Frame, const QSizeF &Size, quint64 TimeNow, bool ResetVideo)
{
    (void)TimeNow;
    (void)Size;

    if (ResetVideo)
        Reset();

    if (!Frame || !m_openGLContext)
        return false;

    if (Frame->m_corrupt && !m_ignoreCorruptFrames)
    {
        m_corruptFrameCount++;
        LOG(VB_GENERAL, LOG_INFO, "Ignoring frame flagged as corrupt");

        if (m_corruptFrameCount > 20 /*an entirely arbitrary number*/)
        {
            m_ignoreCorruptFrames = true;
            LOG(VB_GENERAL, LOG_WARNING, QString("%1 corrupt frames seen - disabling corrupt frame filter").arg(m_corruptFrameCount));
        }
        else
        {
            return false;
        }
    }

    // update the colourspace for the latest frame
    m_videoColourSpace->SetColourSpace(Frame->m_colourSpace);
    m_videoColourSpace->SetColourRange(Frame->m_colourRange);

    // check for format changes
    bool framesizechanged  = m_lastFrameWidth != Frame->m_rawWidth  || m_lastFrameHeight != Frame->m_rawHeight;

    // notify parent of any geometry changes
    bool inverted = (Frame->m_invertForSource + Frame->m_invertForDisplay) & 1;
    if (framesizechanged || (m_lastFrameAspectRatio != Frame->m_frameAspectRatio) || (m_lastFrameInverted != inverted))
    {
        LOG(VB_GENERAL, LOG_INFO, "Video frame geometry changed");
        m_dirtyGeometry = true;
    }

    if (framesizechanged || m_lastInputFormat != Frame->m_pixelFormat)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Video frame format changed from '%1'@%2x%3 to '%4'@%5x%6")
            .arg(av_get_pix_fmt_name(m_lastInputFormat)).arg(m_lastFrameWidth).arg(m_lastFrameHeight)
            .arg(av_get_pix_fmt_name(Frame->m_pixelFormat)).arg(Frame->m_rawWidth).arg(Frame->m_rawHeight));

        Reset();

        // agree custom surface format for hardware acceleration
        int customformat = 0;
        AccelerationFactory* factory = AccelerationFactory::GetAccelerationFactory();
        for ( ; factory; factory = factory->NextFactory())
            if (factory->NeedsCustomSurfaceFormat(Frame, &customformat))
                break;

        m_rgbVideoTextureType = customformat != 0 ? customformat : m_rgbVideoTextureTypeDefault;
    }

    // update frame display attributes
    m_lastInputFormat      = Frame->m_pixelFormat;
    m_lastFrameWidth       = Frame->m_rawWidth;
    m_lastFrameHeight      = Frame->m_rawHeight;
    m_lastFrameAspectRatio = Frame->m_frameAspectRatio;
    m_lastFrameInverted    = inverted;

    // We always create an rgb framebuffer for the video texture, even though some hardware decoding
    // methods only need the actual texture and not the framebuffer 'wrapper'.
    if (!m_rgbVideoFrameBuffer && m_haveFramebuffers)
    {
        // determine the required size
        m_rgbVideoTextureSizeUsed = QSize(Frame->m_rawWidth, Frame->m_rawHeight);
        m_rgbVideoTextureSize     = OpenGLTextureSize(m_rgbVideoTextureSizeUsed, m_useNPOTTextures || m_useRectangularTextures);

        // create the framebuffer
        m_rgbVideoFrameBuffer = new QOpenGLFramebufferObject(m_rgbVideoTextureSize, m_rgbVideoTextureType);

        if (!m_rgbVideoFrameBuffer || (m_rgbVideoFrameBuffer && !m_rgbVideoFrameBuffer->isValid()))
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to create RGB video framebuffer");
            return false;
        }

        // NB wrap has already been set and the texture cleared by Qt
        GLuint texture = m_rgbVideoFrameBuffer->texture();
        glBindTexture(m_rgbVideoTextureType, texture);

        // set the filters
        glTexParameteri(m_rgbVideoTextureType, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(m_rgbVideoTextureType, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glBindTexture(m_rgbVideoTextureType, 0);

        setFiltering(QSGTexture::Linear);
        setHorizontalWrapMode(QSGTexture::ClampToEdge);
        setVerticalWrapMode(QSGTexture::ClampToEdge);
        setMipmapFiltering(QSGTexture::None);
        updateBindOptions(true);
    }

    // bail out if setup has failed
    if (!m_rgbVideoFrameBuffer)
        return false;

    // actually update the frame
    if (IsHardwarePixelFormat(Frame->m_pixelFormat))
    {
        // AccelerationFactory needs refactoring to remove the GLTexture dependency
        /*
        AccelerationFactory* factory = AccelerationFactory::GetAccelerationFactory();
        for ( ; factory; factory = factory->NextFactory())
        {
            if (factory->UpdateFrame(Frame, m_videoColourSpace, (void*)m_rgbVideoTexture))
            {
                m_validVideoFrame = true;
                break;
            }
        }
        */
    }
    else
    {
        // create a raw video texture if needed
        if (!m_rawVideoTexture)
        {
            m_rawVideoTextureSizeUsed = QSize(Frame->m_rawWidth / 2, Frame->m_rawHeight);
            m_rawVideoTextureSize     = OpenGLTextureSize(m_rawVideoTextureSizeUsed, m_useNPOTTextures);

            glGenTextures(1, &m_rawVideoTexture);
            glBindTexture(GL_TEXTURE_2D, m_rawVideoTexture);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            // clear the texture
            uint buffersize = OpenGLBufferSize(m_rawVideoTextureSize, GL_RGBA, GL_UNSIGNED_BYTE);
            if (buffersize > 0)
            {
                unsigned char *scratch = new unsigned char[buffersize];
                if (scratch)
                {
                    memset(scratch, 0, buffersize);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_rawVideoTextureSize.width(), m_rawVideoTextureSize.height(),
                                 0, GL_RGBA, GL_UNSIGNED_BYTE, scratch);
                    delete [] scratch;
                }
            }

            glBindTexture(GL_TEXTURE_2D, 0);
            LOG(VB_GENERAL, LOG_INFO, QString("Created video texture %1x%2").arg(Frame->m_rawWidth).arg(Frame->m_rawHeight));

            emit textureChanged();
        }

        // create a YUV->RGB shader if needed
        if (!m_YUV2RGBShader)
        {
            m_YUV2RGBShader = new QOpenGLShaderProgram();

            QByteArray vertex("GLSL_DEFINES"
                              "attribute mediump vec4 VERTEX;\n"
                              "attribute mediump vec2 TEXCOORDIN;\n"
                              "varying   mediump vec2 TEXCOORDOUT;\n"
                              "uniform   mediump mat4 MATRIX;\n"
                              "void main() {\n"
                              "    gl_Position = MATRIX * VERTEX;\n"
                              "    TEXCOORDOUT = TEXCOORDIN;\n"
                              "}\n"
                              );
            CustomiseShader(vertex, m_rgbVideoTextureType, m_rawVideoTextureSize, m_rawVideoTextureSizeUsed);

            if (m_YUV2RGBShader->addShaderFromSourceCode(QOpenGLShader::Vertex, vertex))
            {
                QByteArray fragment("GLSL_DEFINES"
                                    "uniform GLSL_SAMPLER s_texture0;\n"
                                    "uniform mediump mat4 COLOUR_UNIFORM;\n"
                                    "varying mediump vec2 TEXCOORDOUT;\n"
                                    "void main(void)\n"
                                    "{\n"
                                    "    vec4 yuva = GLSL_TEXTURE(s_texture0, TEXCOORDOUT);\n"
                                    "    if (fract(TEXCOORDOUT.x * SELECT_COLUMN) < 0.5)\n"
                                    "        yuva = yuva.rabg;\n"
                                    "    gl_FragColor = vec4(yuva.arb, 1.0) * COLOUR_UNIFORM;\n"
                                    "}\n"
                                    );

                CustomiseShader(fragment, m_rgbVideoTextureType, m_rawVideoTextureSize, m_rawVideoTextureSizeUsed);

                if (m_YUV2RGBShader->addShaderFromSourceCode(QOpenGLShader::Fragment, fragment))
                {
                    if (m_YUV2RGBShader->link())
                    {
                        LOG(VB_GENERAL, LOG_INFO, "Created YUV->RGB shader");
                        m_YUV2RGBShader->bind();

                        m_YUV2RGBShaderColourLocation = m_YUV2RGBShader->uniformLocation("COLOUR_UNIFORM");

                        // set the orthographix matrix. This only needs to be done once.
                        QRect ortho(0, 0, m_rgbVideoTextureSizeUsed.width(), m_rgbVideoTextureSizeUsed.height());
                        QMatrix4x4 matrix;
                        matrix.ortho(ortho);
                        m_YUV2RGBShader->setUniformValue("MATRIX", matrix);
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_INFO, QString("Shader link error: '%1").arg(m_YUV2RGBShader->log()));
                    }
                }
                else
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("Fragment shader error: '%1'").arg(m_YUV2RGBShader->log()));
                }
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Vertex shader error: '%1'").arg(m_YUV2RGBShader->log()));
            }
        }

        if (!m_YUV2RGBShader || (m_YUV2RGBShader && !m_YUV2RGBShader->isLinked()))
            return false;

        // update raw video texture

        int buffersize = OpenGLBufferSize(QSize(Frame->m_adjustedWidth, Frame->m_adjustedHeight), TORC_UYVY, GL_UNSIGNED_BYTE);

        if (m_conversionBuffer.size() != buffersize)
            m_conversionBuffer.resize(buffersize);

        PixelFormat informat = Frame->m_secondaryPixelFormat != PIX_FMT_NONE ? Frame->m_secondaryPixelFormat : Frame->m_pixelFormat;

        if (informat != m_outputFormat)
        {
            AVPicture in;
            avpicture_fill(&in, Frame->m_buffer, informat, Frame->m_adjustedWidth, Frame->m_adjustedHeight);
            AVPicture out;
            avpicture_fill(&out, (uint8_t*)m_conversionBuffer.data(), m_outputFormat, Frame->m_rawWidth, Frame->m_rawHeight);

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
            memcpy(m_conversionBuffer.data(), Frame->m_buffer, Frame->m_bufferSize);
        }

        glBindTexture(GL_TEXTURE_2D, m_rawVideoTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_rawVideoTextureSizeUsed.width(),
                        m_rawVideoTextureSizeUsed.height(), GL_RGBA,
                        GL_UNSIGNED_BYTE, m_conversionBuffer.data());

        // we have a raw texture, a YUV->RGB shader and a framebuffer. Now do the colourspace conversion.
        // enable framebuffer
        if (m_rgbVideoFrameBuffer->bind())
        {
            // disable blending
            glDisable(GL_BLEND);

            // enable shader
            m_YUV2RGBShader->bind();

            // set the vertices
            m_YUV2RGBShader->enableAttributeArray("VERTEX");
            GLfloat width  = m_rgbVideoTextureSizeUsed.width();
            GLfloat height = m_rgbVideoTextureSizeUsed.height();
            GLfloat const vertices[] = { 0.0f,  height, 0.0f,
                                         0.0f,  0.0f,   0.0f,
                                         width, height, 0.0f,
                                         width, 0.0f,   0.0f };
            m_YUV2RGBShader->setAttributeArray("VERTEX", vertices, 3);

            // set the texture coordindates
            m_YUV2RGBShader->enableAttributeArray("TEXCOORDIN");
            width  = m_rawVideoTextureSizeUsed.width() / m_rawVideoTextureSize.width();
            height = m_rawVideoTextureSizeUsed.height() / m_rawVideoTextureSize.height();
            GLfloat const texcoord[] = { 0.0f,  0.0f,
                                         0.0f,  height,
                                         width, 0.0f,
                                         width, height };
            m_YUV2RGBShader->setAttributeArray("TEXCOORDIN", texcoord, 2);

            // set viewport
            glViewport(0, 0, m_rgbVideoTextureSizeUsed.width(), m_rgbVideoTextureSizeUsed.height());

            // update colourspace
            if (m_videoColourSpace->HasChanged())
            {
                float* data = m_videoColourSpace->Data();
                GLfloat colour[4][4];
                memcpy(colour, data, 16 * sizeof(float));
                m_YUV2RGBShader->setUniformValue(m_YUV2RGBShaderColourLocation, colour);
            }

            // draw
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            // release program
            m_YUV2RGBShader->release();

            // release framebuffer
            m_rgbVideoFrameBuffer->release();
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to bind video framebuffer");
        }

        // release texture
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // the frame has been updated
    return true;
}
