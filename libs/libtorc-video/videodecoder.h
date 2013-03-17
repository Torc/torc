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
class VideoFrame;
class VideoColourSpace;

class VideoDecoder : public AudioDecoder
{
    friend class VideoDecoderFactory;

  public:
    static double  GetFrameAspectRatio (AVStream *Stream, AVFrame &Frame);
    static double  GetPixelAspectRatio (AVStream *Stream, AVFrame &Frame);
    static double  GetFrameRate        (AVFormatContext *Context, AVStream *Stream);

  public:
    ~VideoDecoder();
    AVPixelFormat AgreePixelFormat    (AVCodecContext *Context, const AVPixelFormat *Formats);
    int          GetAVBuffer         (AVCodecContext *Context, AVFrame* Frame);
    void         ReleaseAVBuffer     (AVCodecContext *Context, AVFrame* Frame);

  protected:
    explicit VideoDecoder (const QString &URI, TorcPlayer *Parent, int Flags);

    bool         FilterAudioFrames   (qint64 Timecode);
    bool         VideoBufferStatus   (int &Unused, int &Inuse, int &Held);
    void         ProcessVideoPacket  (AVFormatContext *Context, AVStream *Stream, AVPacket *Packet);
    void         PreInitVideoDecoder (AVFormatContext *Context, AVStream *Stream);
    void         PostInitVideoDecoder(AVCodecContext *Context);
    void         CleanupVideoDecoder (AVStream *Stream);
    void         FlushVideoBuffers   (bool Stopped);

    void         SetFormat           (AVPixelFormat Format, int Width, int Height, int References, bool UpdateParent);

  private:
    void         ResetPTSTracker     (void);
    int64_t      GetValidTimestamp   (int64_t PTS, int64_t DTS);

  private:
    bool         m_keyframeSeen;
    VideoPlayer *m_videoParent;
    AVPixelFormat m_currentPixelFormat;
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

class AccelerationFactory
{
  public:
    AccelerationFactory();
    virtual ~AccelerationFactory();

    static AccelerationFactory* GetAccelerationFactory  (void);
    AccelerationFactory*        NextFactory             (void) const;

    virtual bool                CanAccelerate           (AVCodecContext *Context, AVPixelFormat Format) = 0;
    virtual void                PreInitialiseDecoder    (AVCodecContext *Context) = 0;
    virtual void                PostInitialiseDecoder   (AVCodecContext *Context) = 0;
    virtual void                DeinitialiseDecoder     (AVCodecContext *Context) = 0;
    virtual bool                InitialiseBuffer        (AVCodecContext *Context, AVFrame *Avframe, VideoFrame *Frame) = 0;
    virtual void                DeinitialiseBuffer      (AVCodecContext *Context, AVFrame *Avframe, VideoFrame *Frame) = 0;
    virtual void                ConvertBuffer           (AVFrame &Avframe, VideoFrame *Frame, SwsContext *&ConversionContext) = 0;
    virtual bool                UpdateFrame             (VideoFrame *Frame, VideoColourSpace *ColourSpace, void *Surface) = 0;

  protected:
    static AccelerationFactory* gAccelerationFactory;
    AccelerationFactory*        nextAccelerationFactory;
};

#endif // VIDEODECODER_H
