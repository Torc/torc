#ifndef AUDIOSPDIFENCODER_H
#define AUDIOSPDIFENCODER_H

// Qt
#include <QString>

// Torc
#include "torcaudioexport.h"
#include "audiodecoder.h"
#include "audiooutputlisteners.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavcodec/audioconvert.h"
}

class TORC_AUDIO_PUBLIC AudioSPDIFEncoder
{
  public:
    AudioSPDIFEncoder(QString Muxer, int CodecId);
    ~AudioSPDIFEncoder();

    void           WriteFrame         (unsigned char *Data,   int Size);
    int            GetData            (unsigned char *Buffer, int &Size);
    int            GetProcessedSize   (void);
    unsigned char *GetProcessedBuffer (void);
    void           Reset              (void);
    bool           Succeeded          (void);
    bool           SetMaxHDRate       (int Rate);

  private:
    static int     EncoderCallback   (void *Object, unsigned char *Buffer, int Size);
    void           Destroy           (void);

  private:
    bool             m_complete;
    AVFormatContext *m_formatContext;
    AVStream        *m_stream;
    unsigned char    m_buffer[MAX_AUDIO_FRAME_SIZE];
    long             m_size;
};

#endif
