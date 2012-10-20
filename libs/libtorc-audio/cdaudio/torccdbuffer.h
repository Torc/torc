#ifndef TORCCDBUFFER_H
#define TORCCDBUFFER_H

// Torc
#include "torcbuffer.h"

extern "C" {
#include "libavformat/avformat.h"
}

class TorcCDBuffer : public TorcBuffer
{
  public:
    explicit TorcCDBuffer(const QString &URI);
    ~TorcCDBuffer();

    bool       Open               (void);
    void       Close              (void);
    void*      RequiredAVFormat   (void);
    QString    GetFilteredUri     (void);
    int        Read               (quint8 *Buffer, qint32 BufferSize);
    int        Peek               (quint8 *Buffer, qint32 BufferSize);
    int        Write              (quint8 *Buffer, qint32 BufferSize);
    int64_t    Seek               (int64_t  Offset, int Whence);
    qint64     GetSize            (void);
    qint64     GetPosition        (void);
    bool       IsSequential       (void);
    qint64     BytesAvailable     (void);
    int        BestBufferSize     (void);

  private:
    AVInputFormat *m_input;
};

#endif // TORCCDBUFFER_H
