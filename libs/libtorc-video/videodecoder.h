#ifndef VIDEODECODER_H
#define VIDEODECODER_H

// Torc
#include "audiodecoder.h"

extern "C" {
#include "libavutil/pixfmt.h"
#include "libavutil/pixdesc.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
}

class VideoPlayer;

class VideoDecoder : public AudioDecoder
{
    friend class VideoDecoderFactory;

  public:
    static double  GetFrameAspectRatio (AVStream *Stream, AVFrame &Frame);
    static double  GetPixelAspectRatio (AVStream *Stream, AVFrame &Frame);
    static double  GetFrameRate        (AVFormatContext *Context, AVStream *Stream);

  public:
    ~VideoDecoder();
    PixelFormat  AgreePixelFormat    (AVCodecContext *Context, const PixelFormat *Formats);
    int          GetAVBuffer         (AVCodecContext *Context, AVFrame* Frame);
    void         ReleaseAVBuffer     (AVCodecContext *Context, AVFrame* Frame);

  protected:
    explicit VideoDecoder (const QString &URI, TorcPlayer *Parent, int Flags);

    bool         FilterAudioFrames   (qint64 Timecode);
    bool         VideoBufferStatus   (int &Unused, int &Inuse, int &Held);
    void         ProcessVideoPacket  (AVFormatContext *Context, AVStream *Stream, AVPacket *Packet);
    void         SetupVideoDecoder   (AVFormatContext *Context, AVStream *Stream);
    void         CleanupVideoDecoder (AVStream *Stream);
    void         FlushVideoBuffers   (bool Stopped);

    void         SetFormat           (PixelFormat Format, int Width, int Height, int References, bool UpdateParent);

  private:
    void         ResetPTSTracker     (void);
    int64_t      GetValidTimestamp   (int64_t PTS, int64_t DTS);

  private:
    bool         m_keyframeSeen;
    VideoPlayer *m_videoParent;
    PixelFormat  m_currentPixelFormat;
    int          m_currentVideoWidth;
    int          m_currentVideoHeight;
    int          m_currentReferenceCount;
    SwsContext  *m_conversionContext;

    int64_t      m_faultyPTSCount;
    int64_t      m_faultyDTSCount;
    int64_t      m_lastPTS;
    int64_t      m_lastDTS;

    bool         m_filterAudioFrames;
    int64_t      m_firstVideoTimecode;
};

#endif // VIDEODECODER_H
