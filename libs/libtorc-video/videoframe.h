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
  public:
    enum Field
    {
        Frame       = 0,
        TopField    = 1,
        BottomField = 2
    };

    friend class VideoDecoder;

  public:
    static int     PlaneCount    (AVPixelFormat Format);

  public:
    explicit VideoFrame          (AVPixelFormat PreferredDisplayFormat);
    ~VideoFrame                  ();

    void           Reset         (void);
    void           Initialise    (AVPixelFormat Format, int Width, int Height);
    void           InitialiseBuffer (void);
    bool           Discard       (void);
    void           SetDiscard    (void);
    void           SetOffsets    (void);

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
    int            m_bufferSize;
    AVPixelFormat  m_pixelFormat;
    AVPixelFormat  m_secondaryPixelFormat;
    AVPixelFormat  m_preferredDisplayFormat;

    int            m_invertForSource;
    int            m_invertForDisplay;
    AVColorSpace   m_colourSpace;
    AVColorRange   m_colourRange;
    Field          m_field;
    bool           m_topFieldFirst;
    bool           m_interlaced;
    double         m_frameAspectRatio;
    double         m_pixelAspectRatio;
    bool           m_repeatPict;
    qint64         m_frameNumber;
    qint64         m_pts;
    bool           m_corrupt;
    double         m_frameRate;

    void*          m_acceleratedBuffer;
};

#endif // VIDEOFRAME_H
