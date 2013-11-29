#ifndef TORCSGVIDEOPROVIDER_H
#define TORCSGVIDEOPROVIDER_H

// Qt
#include <QSGTexture>
#include <QOpenGLContext>
#include <QSGTextureProvider>

// Torc
#include "torcqmlexport.h"
#include "torcqmlopengldefs.h"

extern "C" {
#include "libavutil/pixfmt.h"
#include "libswscale/swscale.h"
}

class VideoFrame;
class VideoColourSpace;
class QOpenGLBuffer;
class QOpenGLShaderProgram;
class QOpenGLFramebufferObject;

class TORC_QML_PUBLIC TorcSGVideoProvider : public QSGTexture, public QSGTextureProvider
{
  public:
    TorcSGVideoProvider(VideoColourSpace *ColourSpace);
    virtual ~TorcSGVideoProvider();

    bool                Refresh               (VideoFrame* Frame, const QSizeF &Size, quint64 TimeNow, bool ResetVideo);
    void                Reset                 (void);
    qreal               GetVideoAspectRatio   (void);
    QSizeF              GetVideoSize          (void);
    QRectF              GetCachedGeometry     (void);
    QRectF              GetGeometry           (const QRectF &ParentGeometry, qreal DisplayAspectRatio);
    bool                GetDirtyGeometry      (void);

    // QSGTextureProvider
    QSGTexture*         texture               (void) const;

    // QSGTexture
    int                 textureId             (void) const;
    QSize               textureSize           (void) const;
    bool                hasAlphaChannel       (void) const;
    bool                hasMipmaps            (void) const;
    void                bind                  (void);

  private:
    void                CustomiseTextures     (void);
    void                CustomiseShader       (QByteArray &Source, GLenum TextureType, QSize &Size, QSize &UsedSize);

  private:
    GLuint              m_rawVideoTexture;
    QSize               m_rawVideoTextureSize;
    QSize               m_rawVideoTextureSizeUsed;
    QOpenGLBuffer      *m_rawVideoTextureBuffer;
    QOpenGLShaderProgram *m_YUV2RGBShader;
    int                 m_YUV2RGBShaderColourLocation;
    int                 m_YUV2RGBShaderTextureLocation;
    int                 m_YUV2RGBShaderVertexLocation;

    QOpenGLFramebufferObject *m_rgbVideoFrameBuffer;
    GLenum              m_rgbVideoTextureType;
    GLenum              m_rgbVideoTextureTypeDefault;
    QSize               m_rgbVideoTextureSize;
    QSize               m_rgbVideoTextureSizeUsed;

    QOpenGLContext     *m_openGLContext;
    bool                m_haveNPOTTextures;
    bool                m_useNPOTTextures;
    bool                m_haveRectangularTextures;
    bool                m_useRectangularTextures;
    bool                m_needRectangularTextures;
    bool                m_haveFramebuffers;
    QByteArray          m_qualifiers;
    QByteArray          m_GLSLVersion;

    bool                m_ignoreCorruptFrames;
    int                 m_corruptFrameCount;
    AVPixelFormat       m_outputFormat;
    AVPixelFormat       m_lastInputFormat;
    double              m_lastFrameAspectRatio;
    int                 m_lastFrameWidth;
    int                 m_lastFrameHeight;
    bool                m_lastFrameInverted;
    bool                m_dirtyGeometry;
    VideoColourSpace   *m_videoColourSpace;
    SwsContext         *m_conversionContext;
    QByteArray          m_conversionBuffer;

    QRectF              m_cachedVideoGeometry;
};


#endif // TORCSGVIDEOPROVIDER_H
