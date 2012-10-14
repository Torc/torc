#ifndef VIDEODECODER_H
#define VIDEODECODER_H

// Torc
#include "audiodecoder.h"

class VideoDecoder : public AudioDecoder
{
    friend class VideoDecoderFactory;

  public:
    ~VideoDecoder();

  protected:
    explicit VideoDecoder (const QString &URI, TorcPlayer *Parent, int Flags);
};

#endif // VIDEODECODER_H
