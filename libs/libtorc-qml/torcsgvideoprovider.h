#ifndef TORCSGVIDEOPROVIDER_H
#define TORCSGVIDEOPROVIDER_H

#include <QSGTexture>
#include <QOpenGLContext>
#include <QSGTextureProvider>

// Qt
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLFramebufferObject>

// Torc
#include "torcqmlexport.h"
#include "torcqmlopengldefs.h"

extern "C" {
#include "libavutil/pixfmt.h"
#include "libswscale/swscale.h"
}

class VideoFrame;
class VideoColourSpace;

class TORC_QML_PUBLIC TorcSGVideoProvider : public QSGTexture, public QSGTextureProvider
{
  public:
    TorcSGVideoProvider(VideoColourSpace *ColourSpace);
    virtual ~TorcSGVideoProvider();

    bool                Refresh               (VideoFrame* Frame, const QSizeF &Size, quint64 TimeNow);
    void                Reset                 (void);


    // QSGTextureProvider
    QSGTexture*         texture               (void) const;

    // QSGTexture
    int                 textureId             (void) const;
    QSize               textureSize           (void) const;
    bool                hasAlphaChannel       (void) const;
    bool                hasMipmaps            (void) const;
    void                bind                  (void);

  private:
    void                CustomiseShader       (QByteArray &Source, GLenum TextureType, QSize &Size, QSize &UsedSize);

  private:
    GLuint              m_rawVideoTexture;
    QSize               m_rawVideoTextureSize;
    QSize               m_rawVideoTextureSizeUsed;
    QOpenGLShaderProgram *m_YUV2RGBShader;
    int                 m_YUV2RGBShaderColourLocation;

    QOpenGLFramebufferObject *m_rgbVideoFrameBuffer;
    GLenum              m_rgbVideoTextureType;
    QSize               m_rgbVideoTextureSize;
    QSize               m_rgbVideoTextureSizeUsed;

    QOpenGLContext     *m_openGLContext;
    bool                m_useNPOTTextures;
    bool                m_haveFramebuffers;
    QByteArray          m_qualifiers;
    QByteArray          m_GLSLVersion;

    AVPixelFormat       m_outputFormat;
    AVPixelFormat       m_lastInputFormat;
    bool                m_validVideoFrame;
    double              m_lastFrameAspectRatio;
    int                 m_lastFrameWidth;
    int                 m_lastFrameHeight;
    VideoColourSpace   *m_videoColourSpace;
    SwsContext         *m_conversionContext;
    QByteArray          m_conversionBuffer;
};


#endif // TORCSGVIDEOPROVIDER_H
