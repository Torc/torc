#ifndef VIDEOFRAME_H
#define VIDEOFRAME_H

// Qt
#include <QtGlobal>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/pixfmt.h"
}

class VideoFrame
{
    friend class VideoDecoder;

  public:
    static int     PlaneCount    (PixelFormat Format);

  public:
    explicit VideoFrame();
    ~VideoFrame();

    void           Reset         (void);
    bool           Initialise    (PixelFormat Format, int Width, int Height);
    bool           Discard       (void);
    void           SetDiscard    (void);
    bool           SetOffsets    (void);

    unsigned char *m_buffer;
    int            m_pitches[4];
    int            m_offsets[4];
    unsigned char *m_priv[4];

    bool           m_discard;
    int            m_rawWidth;
    int            m_rawHeight;
    int            m_adjustedWidth;
    int            m_adjustedHeight;
    int            m_bitsPerPixel;
    int            m_numPlanes;
    PixelFormat    m_pixelFormat;

    AVColorSpace   m_colorSpace;
    bool           m_topFieldFirst;
    bool           m_interlaced;
    double         m_aspectRatio;
    bool           m_repeatPict;
    qint64         m_frameNumber;
    qint64         m_pts;
    qint64         m_dts;
};

#endif // VIDEOFRAME_H
