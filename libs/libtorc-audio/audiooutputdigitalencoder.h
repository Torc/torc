#ifndef AUDIOOUTPUTREENCODER_H
#define AUDIOOUTPUTREENCODER_H

// Torc
#include "audiooutputsettings.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

#define INBUFSIZE  131072
#define OUTBUFSIZE INBUFSIZE

class AudioSPDIFEncoder;

class AudioOutputDigitalEncoder
{
  public:
    AudioOutputDigitalEncoder();
   ~AudioOutputDigitalEncoder();

    bool   Init       (AVCodecID CodecId, int Bitrate, int Samplerate, int Channels);
    void   Dispose    (void);
    size_t Encode     (void *Buffer, int Length, AudioFormat Format);
    size_t GetFrames  (void *Pointer, int MaxLength);
    int    Buffered   (void) const;
    void   Clear      (void);

  private:
    void*  Reallocate (void *Pointer, size_t OldSize, size_t NewSize);

    AVCodecContext    *m_avContext;
    qint16            *m_outBuffer;
    size_t             m_outSize;
    qint16            *m_inBuffer;
    size_t             m_inSize;
    int                m_outLength;
    int                m_inLength;
    size_t             m_samplesPerFrame;
    qint16             m_encodebuffer[FF_MIN_BUFFER_SIZE];
    AudioSPDIFEncoder *m_spdifEncoder;
};

#endif
