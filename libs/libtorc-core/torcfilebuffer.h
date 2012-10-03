#ifndef TORCFILEBUFFER_H
#define TORCFILEBUFFER_H

// Qt
#include <QFile>

// Torc
#include "torcbuffer.h"

class TorcFileBuffer : public TorcBuffer
{
  public:
    explicit TorcFileBuffer(const QString &URI);
    ~TorcFileBuffer();

    bool   Open            (void);
    void   Close           (void);
    int    Read            (quint8 *Buffer, qint32 BufferSize);
    int    Peek            (quint8 *Buffer, qint32 BufferSize);
    int    Write           (quint8 *Buffer, qint32 BufferSize);
    qint64 Seek            (qint64  Offset, int Whence);
    qint64 GetSize         (void);
    qint64 GetPosition     (void);
    bool   IsSequential    (void);
    qint64 BytesAvailable  (void);

  private:
    QFile   *m_file;
};

#endif // TORCFILEBUFFER_H
