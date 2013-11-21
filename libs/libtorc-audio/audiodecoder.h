#ifndef AUDIODECODER_H
#define AUDIODECODER_H

// Qt
#include <QMap>
#include <QReadWriteLock>

// Torc
#include "torcaudioexport.h"
#include "torclanguage.h"
#include "torcdecoder.h"

class AudioWrapper;
class AudioDescription;
class TorcPlayer;
class TorcChapter;
class TorcStreamData;
class TorcProgramData;
class AudioDecoderPriv;
class TorcDecoderThread;
class TorcDemuxerThread;
class TorcVideoThread;
class TorcAudioThread;
class TorcSubtitleThread;

extern "C" {
#include "libavformat/avformat.h"
}

#define MAX_AUDIO_FRAME_SIZE 192000

class TORC_AUDIO_PUBLIC AudioDecoder : public TorcDecoder
{
    friend class TorcAudioDecoderFactory;
    friend class TorcDemuxerThread;
    friend class TorcVideoThread;
    friend class TorcAudioThread;
    friend class TorcSubtitleThread;

  public:
    static void      InitialiseLibav    (void);
    static QString   StreamTypeToString (TorcStreamTypes Type);
    static int       DecoderInterrupt   (void* Object);

  public:
    virtual         ~AudioDecoder       ();

    // Public API
    bool             HandleAction       (int Action);
    bool             Open               (void);
    TorcDecoder::DecoderState GetState  (void);
    TorcDecoder::DemuxerState GetDemuxerState (void);
    void             SetDemuxerState    (TorcDecoder::DemuxerState State);
    void             Start              (void);
    void             Pause              (void);
    void             Stop               (void);
    void             Seek               (void);
    int              GetCurrentStream   (TorcStreamTypes Type);
    int              GetStreamCount     (TorcStreamTypes Type);

  protected:
    explicit         AudioDecoder       (const QString &URI, TorcPlayer *Parent, int Flags);
    void             SetupAudio         (TorcAudioThread    *Thread);
    bool             PauseDecoderThreads(TorcDemuxerThread  *Thread);
    void             ResetStreams       (TorcDemuxerThread  *Thread);
    bool             CreateAVFormatContext (TorcDemuxerThread *Thread);
    void             DeleteAVFormatContext (TorcDemuxerThread *Thread);
    bool             OpenDemuxer        (TorcDemuxerThread  *Thread);
    bool             ResetDemuxer       (TorcDemuxerThread  *Thread);
    bool             UpdateDemuxer      (TorcDemuxerThread  *Thread);
    void             CloseDemuxer       (TorcDemuxerThread  *Thread);
    void             DemuxPackets       (TorcDemuxerThread  *Thread);
    void             DecodeAudioFrames  (TorcAudioThread    *Thread);
    void             DecodeVideoFrames  (TorcVideoThread    *Thread);
    void             DecodeSubtitles    (TorcSubtitleThread *Thread);

    virtual bool     FilterAudioFrames  (qint64 Timecode);
    virtual bool     VideoBufferStatus  (int &Unused, int &Inuse, int &Held);
    virtual void     ProcessVideoPacket (AVFormatContext *Context, AVStream* Stream, AVPacket *Packet);
    virtual AVCodec* PreInitVideoDecoder(AVFormatContext *Context, AVStream* Stream);
    virtual void     PostInitVideoDecoder(AVCodecContext *Context);
    virtual void     CleanupVideoDecoder(AVStream* Stream);
    virtual void     FlushVideoBuffers  (bool Stopped);

  private:
    void             TearDown           (void);
    void             SetFlag            (TorcDecoder::DecoderFlags Flag);
    bool             FlagIsSet          (TorcDecoder::DecoderFlags Flag);
    bool             SelectProgram      (int Index);
    bool             SelectStreams      (void);
    bool             OpenDecoders       (void);
    void             CloseDecoders      (void);
    bool             ScanPrograms       (void);
    void             ResetPrograms      (void);
    TorcProgramData* ScanProgram        (uint Index);
    TorcStreamData*  ScanStream         (uint Index);
    void             ScanChapters       (void);
    void             ResetChapters      (void);
    bool             SelectStream       (TorcStreamTypes Type);
    void             UpdateBitrate      (void);
    bool             OpenDecoders       (const QList<TorcStreamData*> &Streams);
    void             DebugPrograms      (void);
    void             DebugStreams       (const QList<TorcStreamData*> &Streams);

  protected:
    TorcPlayer                          *m_parent;

    AudioWrapper                        *m_audio;
    AudioDescription                    *m_audioIn;
    AudioDescription                    *m_audioOut;

    int                                  m_interruptDecoder;
    QString                              m_uri;
    int                                  m_flags;
    AudioDecoderPriv                    *m_priv;
    bool                                 m_seek;
    double                               m_duration;
    int                                  m_bitrate;
    int                                  m_bitrateFactor;

    QReadWriteLock                      *m_streamLock;
    int                                  m_currentStreams[StreamTypeEnd];
    int                                  m_currentProgram;
    QList<TorcProgramData*>              m_programs;
    QMap<QString,QString>                m_avMetaData;

    QReadWriteLock                      *m_chapterLock;
    QList<TorcChapter*>                  m_chapters;
};

#endif
