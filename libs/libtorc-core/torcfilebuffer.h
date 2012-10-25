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

    bool     Open            (void);
    void     Close           (void);
    int      Read            (quint8 *Buffer, qint32 BufferSize);
    int      Peek            (quint8 *Buffer, qint32 BufferSize);
    int      Write           (quint8 *Buffer, qint32 BufferSize);
    int64_t  Seek            (int64_t  Offset, int Whence);
    QString  GetFilteredPath (void);
    QByteArray ReadAll       (void);
    qint64   GetSize         (void);
    qint64   GetPosition     (void);
    bool     IsSequential    (void);
    qint64   BytesAvailable  (void);
    int      BestBufferSize  (void);

  private:
    QFile   *m_file;
    QString  m_path;
};

#endif // TORCFILEBUFFER_H
