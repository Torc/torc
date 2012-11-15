#ifndef VIDEODECODER_H
#define VIDEODECODER_H

// Torc
#include "audiodecoder.h"

class VideoPlayer;

class VideoDecoder : public AudioDecoder
{
    friend class VideoDecoderFactory;

  public:
    ~VideoDecoder();

  protected:
    explicit VideoDecoder (const QString &URI, TorcPlayer *Parent, int Flags);

    bool         VideoBufferStatus   (int &Unused, int &Inuse, int &Held);
    void         ProcessVideoPacket  (AVStream *Stream, AVPacket *Packet);
    void         SetupVideoDecoder   (AVStream *Stream);
    void         FlushVideoBuffers   (void);

  private:
    VideoPlayer *m_videoParent;
    PixelFormat  m_currentPixelFormat;
    int          m_currentVideoWidth;
    int          m_currentVideoHeight;
    int          m_currentReferenceCount;
};

#endif // VIDEODECODER_H
