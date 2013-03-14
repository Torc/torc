#ifndef VIDEOBUFFERS_H
#define VIDEOBUFFERS_H

// Qt
#include <QMutex>
#include <QList>

extern "C" {
#include "libavutil/pixfmt.h"
}

#define MIN_VIDEO_BUFFERS_FOR_DECODE  2
#define MIN_VIDEO_BUFFERS_FOR_DISPLAY 6

class VideoFrame;

class VideoBuffers
{
  public:
    VideoBuffers();
    ~VideoBuffers();

    void               Debug                      (void);
    void               FormatChanged              (AVPixelFormat Format, int Width, int Height, int ReferenceFrames = 2);
    void               Reset                      (bool DeleteFrames);
    bool               GetBufferStatus            (int &Unused, int &Inuse, int &Held);
    void               SetDisplayFormat           (AVPixelFormat Format);
    bool               GetNextVideoTimeStamp      (qint64 &TimeStamp, int WaitUSecs = 0);

    VideoFrame*        GetFrameForDecoding        (void);
    void               ReleaseFrameFromDecoding   (VideoFrame *Frame);
    void               ReleaseFrameFromDecoded    (VideoFrame *Frame);
    VideoFrame*        GetFrameForDisplaying      (int WaitUSecs = 0);
    void               ReleaseFrameFromDisplaying (VideoFrame *Frame, bool InUseForDeinterlacer);
    void               ReleaseFrameFromDisplayed  (VideoFrame *Frame);
    void               CheckDecodedFrames         (void);

  private:
    void               SetFormat                  (AVPixelFormat Format, int Width, int Height);

  private:
    int                m_frameCount;
    int                m_referenceFrames;
    int                m_displayFrames;
    QMutex            *m_lock;

    QList<VideoFrame*> m_unused;
    QList<VideoFrame*> m_decoding;
    QList<VideoFrame*> m_ready;
    QList<VideoFrame*> m_displaying;
    QList<VideoFrame*> m_displayed;
    QList<VideoFrame*> m_reference;

    QList<VideoFrame*> m_decoded;

    AVPixelFormat      m_currentFormat;
    int                m_currentWidth;
    int                m_currentHeight;

    AVPixelFormat      m_preferredDisplayFormat;
};

#endif // VIDEOBUFFERS_H
