#ifndef TORCBLURAYBUFFER_H
#define TORCBLURAYBUFFER_H

// Torc
#include "torcbuffer.h"
#include "torcvideoexport.h"

class TorcBlurayHandler;

class TORC_VIDEO_PUBLIC TorcBlurayBuffer : public TorcBuffer
{
  public:
    TorcBlurayBuffer(void *Parent, const QString &URI, int *Abort);
    virtual ~TorcBlurayBuffer();

    virtual bool  Open                 (void);
    void          Close                (void);
    bool          IgnoreEOF            (void);
    QString       GetFilteredUri       (void);
    int           Read                 (quint8 *Buffer, qint32 BufferSize);
    int           Peek                 (quint8 *Buffer, qint32 BufferSize);
    int           Write                (quint8 *Buffer, qint32 BufferSize);
    int64_t       Seek                 (int64_t  Offset, int Whence);
    qint64        GetSize              (void);
    qint64        GetPosition          (void);
    bool          IsSequential         (void);
    qint64        BytesAvailable       (void);
    int           BestBufferSize       (void);

  protected:
    TorcBlurayHandler *m_handler;
};

#endif // TORCBLURAYBUFFER_H
