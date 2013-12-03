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
    static TorcBuffer* Create             (void* Parent, const QString &URI, int *Abort, bool Media = false);
    static int         StaticRead         (void* Object, quint8* Buffer, qint32 BufferSize);
    static int         StaticWrite        (void* Object, quint8* Buffer, qint32 BufferSize);
    static int64_t     StaticSeek         (void* Object, int64_t Offset, int Whence);

    virtual bool       Open               (void);
    virtual void       Close              (void);
    virtual bool       IgnoreEOF          (void);
    virtual bool       HandleAction       (int Action);
    virtual void*      RequiredAVContext  (void);
    virtual void       InitialiseAVContext(void* Context);
    virtual void*      RequiredAVFormat   (bool &BufferRequired);
    virtual int        Read               (quint8 *Buffer, qint32 BufferSize) = 0;
    virtual int        Peek               (quint8 *Buffer, qint32 BufferSize) = 0;
    virtual int        Write              (quint8 *Buffer, qint32 BufferSize) = 0;
    virtual int64_t    Seek               (int64_t Offset, int Whence) = 0;
    virtual qint64     GetSize            (void) = 0;
    virtual qint64     GetPosition        (void) = 0;
    virtual bool       IsSequential       (void) = 0;
    virtual qint64     BytesAvailable     (void) = 0;
    virtual int        BestBufferSize     (void) = 0;
    virtual QByteArray ReadAll            (int Timeout = 0);
    virtual bool       Pause              (void);
    virtual bool       Unpause            (void);
    virtual bool       TogglePause        (void);
    virtual QString    GetFilteredUri     (void);
    virtual QString    GetPath            (void);
    QString            GetURI             (void);
    bool               GetPaused          (void);
    void*              GetParent          (void);
    void               SetBitrate         (int Bitrate, int Factor);

  protected:
    explicit           TorcBuffer(void* Parent, const QString &URI, int *Abort);

  protected:
    void              *m_parent;
    QString            m_uri;
    QString            m_path;
    BufferState        m_state;
    bool               m_paused;
    int                m_bitrate;
    int                m_bitrateFactor;
    int               *m_abort;
};

class TORC_CORE_PUBLIC TorcBufferFactory
{
  public:
    TorcBufferFactory();
    virtual ~TorcBufferFactory();
    static TorcBufferFactory* GetTorcBufferFactory  (void);
    TorcBufferFactory*        NextTorcBufferFactory (void) const;
    virtual void              Score                 (const QString &URI,
                                                     const QUrl    &URL,
                                                     int           &Score,
                                                     const bool    &Media) = 0;
    virtual TorcBuffer*       Create                (void          *Parent,
                                                     const QString &URI,
                                                     const QUrl    &URL,
                                                     const int     &Score,
                                                     int           *Abort,
                                                     const bool    &Media) = 0;

  protected:
    static TorcBufferFactory* gTorcBufferFactory;
    TorcBufferFactory*        m_nextTorcBufferFactory;
};

#endif
