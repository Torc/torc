#ifndef TORCRAOPBUFFER_H
#define TORCRAOPBUFFER_H

// Torc
#include "torcbuffer.h"
#include "torcraopconnection.h"

extern "C" {
#include "libavformat/avformat.h"
}

class TorcRAOPBuffer : public TorcBuffer
{
  public:
    explicit TorcRAOPBuffer(void *Parent, const QString &URI, const QUrl &URL, int *Abort);
    virtual ~TorcRAOPBuffer();

    bool       Open               (void);
    void       Close              (void);
    void*      RequiredAVContext (void);
    int        Read               (quint8 *Buffer, qint32 BufferSize);
    int        Peek               (quint8 *Buffer, qint32 BufferSize);
    int        Write              (quint8 *Buffer, qint32 BufferSize);
    int64_t    Seek               (int64_t Offset, int Whence);
    qint64     GetSize            (void);
    qint64     GetPosition        (void);
    bool       IsSequential       (void);
    qint64     BytesAvailable     (void);
    int        BestBufferSize     (void);
    bool       Pause              (void);
    bool       Unpause            (void);
    bool       TogglePause        (void);

  private:
    QUrl             m_url;
    int              m_streamId;
    int              m_frameSize;
    int              m_channels;
    int              m_sampleSize;
    int              m_historyMult;
    int              m_initialHistory;
    int              m_kModifier;
    AVStream        *m_stream;
    AVFormatContext *m_avFormatContext;
};

#endif // TORCRAOPBUFFER_H
