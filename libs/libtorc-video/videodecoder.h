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
    ~VideoDecoder();
    PixelFormat  AgreePixelFormat    (AVCodecContext *Context, const PixelFormat *Formats);
    int          GetAVBuffer         (AVCodecContext *Context, AVFrame* Frame);
    void         ReleaseAVBuffer     (AVCodecContext *Context, AVFrame* Frame);

  protected:
    explicit VideoDecoder (const QString &URI, TorcPlayer *Parent, int Flags);

    bool         VideoBufferStatus   (int &Unused, int &Inuse, int &Held);
    void         ProcessVideoPacket  (AVFormatContext *Context, AVStream *Stream, AVPacket *Packet);
    void         SetupVideoDecoder   (AVStream *Stream);
    void         CleanupVideoDecoder (AVStream *Stream);
    void         FlushVideoBuffers   (bool Stopped);

    void         SetFormat           (PixelFormat Format, int Width, int Height, int References, bool UpdateParent);

  private:
    bool         m_keyframeSeen;
    VideoPlayer *m_videoParent;
    PixelFormat  m_currentPixelFormat;
    int          m_currentVideoWidth;
    int          m_currentVideoHeight;
    int          m_currentReferenceCount;
    SwsContext  *m_conversionContext;
};

#endif // VIDEODECODER_H
