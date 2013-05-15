#ifndef TORCNETWORKBUFFER_H
#define TORCNETWORKBUFFER_H

// Qt
#include <QNetworkRequest>
#include <QNetworkReply>

// Torc
#include "torcbuffer.h"
#include "torcnetwork.h"

class TORC_CORE_PUBLIC TorcNetworkBuffer : public TorcBuffer
{
  public:
    enum Type
    {
        Unbuffered, // straight download
        Buffered,   // buffered but not seekable
        Streamed    // buffered and seekable
    };

  public:
    explicit TorcNetworkBuffer(const QString &URI, bool Media, int *Abort);
    ~TorcNetworkBuffer();

    bool     Open            (void);
    void     Close           (void);
    int      Read            (quint8 *Buffer, qint32 BufferSize);
    int      Peek            (quint8 *Buffer, qint32 BufferSize);
    int      Write           (quint8 *Buffer, qint32 BufferSize);
    int64_t  Seek            (int64_t  Offset, int Whence);
    QByteArray ReadAll       (int Timeout = 0);
    qint64   GetSize         (void);
    qint64   GetPosition     (void);
    bool     IsSequential    (void);
    qint64   BytesAvailable  (void);
    int      BestBufferSize  (void);
    QString  GetPath         (void);

  private:
    bool                     m_media;
    Type                     m_type;
    TorcNetworkRequest      *m_request;
};

#endif // TORCNETWORKBUFFER_H
