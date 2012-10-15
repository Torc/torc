#ifndef TORCBUFFER_H
#define TORCBUFFER_H

// Qt
#include <QUrl>
#include <QString>

// Torc
#include "torccompat.h"
#include "torccoreexport.h"

// this avoids a dependancy on libavformat
#ifndef AVSEEK_SIZE
#define AVSEEK_SIZE 0x10000
#endif
#ifndef AVSEEK_FORCE
#define AVSEEK_FORCE 0x20000
#endif

class TORC_CORE_PUBLIC TorcBuffer
{
  public:
    typedef enum
    {
        Status_Created,
        Status_Opening,
        Status_Opened,
        Status_Pausing,
        Status_Paused,
        Status_Closing,
        Status_Closed,
        Status_Destroyed,
        Status_Invalid
    } BufferState;

  public:
    virtual ~TorcBuffer();

    static TorcBuffer* Create             (const QString &URI);
    static int         Read               (void* Object, quint8* Buffer, qint32 BufferSize);
    static int         Write              (void* Object, quint8* Buffer, qint32 BufferSize);
    static qint64      Seek               (void* Object, qint64  Offset, int Whence);

    virtual int        (*GetReadFunction  (void))(void*, quint8*, qint32);
    virtual int        (*GetWriteFunction (void))(void*, quint8*, qint32);
    virtual int64_t    (*GetSeekFunction  (void))(void*, int64_t, int);

    virtual bool       Open               (void);
    virtual void       Close              (void);
    virtual bool       HandleAction       (int Action);
    virtual void*      RequiredAVFormat   (void);
    virtual int        Read               (quint8 *Buffer, qint32 BufferSize) = 0;
    virtual int        Peek               (quint8 *Buffer, qint32 BufferSize) = 0;
    virtual int        Write              (quint8 *Buffer, qint32 BufferSize) = 0;
    virtual int64_t    Seek               (int64_t  Offset, int Whence) = 0;
    virtual qint64     GetSize            (void) = 0;
    virtual qint64     GetPosition        (void) = 0;
    virtual bool       IsSequential       (void) = 0;
    virtual qint64     BytesAvailable     (void) = 0;
    virtual int        BestBufferSize     (void) = 0;
    virtual bool       Pause              (void);
    virtual bool       Unpause            (void);
    virtual bool       TogglePause        (void);
    virtual QString    GetFilteredUri     (void);
    QString            GetURI             (void);
    bool               GetPaused          (void);
    void               SetBitrate         (int Bitrate, int Factor);

  protected:
    explicit           TorcBuffer(const QString &URI);

  protected:
    QString            m_uri;
    BufferState        m_state;
    bool               m_paused;
    int                m_bitrate;
    int                m_bitrateFactor;
};

class TORC_CORE_PUBLIC TorcBufferFactory
{
  public:
    TorcBufferFactory();
    virtual ~TorcBufferFactory();
    static TorcBufferFactory* GetTorcBufferFactory  (void);
    TorcBufferFactory*        NextTorcBufferFactory (void) const;
    virtual TorcBuffer*       Create                (const QString &URI,
                                                     const QUrl    &URL) = 0;

  protected:
    static TorcBufferFactory* gTorcBufferFactory;
    TorcBufferFactory*        m_nextTorcBufferFactory;
};

#endif
