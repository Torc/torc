#ifndef VIDEODECODEROMX_H
#define VIDEODECODEROMX_H

// Torc
#include "openmax/torcomxcore.h"
#include "openmax/torcomxtunnel.h"
#include "openmax/torcomxcomponent.h"
#include "audiodecoder.h"

extern "C" {
#include "libavutil/pixfmt.h"
#include "libavutil/pixdesc.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
}

class VideoDecoderOMX : public AudioDecoder
{
    friend class VideoDecoderOMXFactory;

  public:
   ~VideoDecoderOMX();

  protected:
    VideoDecoderOMX(const QString &URI, TorcPlayer *Parent, int Flags);

    bool              VideoBufferStatus     (int &Unused, int &Inuse, int &Held);
    void              ProcessVideoPacket    (AVFormatContext *Context, AVStream *Stream, AVPacket *Packet);
    void              SetupVideoDecoder     (AVFormatContext *Context, AVStream *Stream);
    void              CleanupVideoDecoder   (AVStream *Stream);
    void              FlushVideoBuffers     (bool Stopped);
    void              SetFormat             (PixelFormat Format, int Width, int Height, int References, bool UpdateParent);

  private:
    TorcOMXCore      *m_core;
    TorcOMXComponent *m_decoder;
    TorcOMXComponent *m_scheduler;
    TorcOMXComponent *m_render;
    TorcOMXComponent *m_clock;
    TorcOMXTunnel    *m_decoderToScheduler;
    TorcOMXTunnel    *m_schedulerToRender;
    TorcOMXTunnel    *m_clockToScheduler;
    VideoPlayer      *m_videoParent;
    PixelFormat       m_currentPixelFormat;
    int               m_currentVideoWidth;
    int               m_currentVideoHeight;
    int               m_currentReferenceCount;
};
#endif // VIDEODECODEROMX_H
