#ifndef AUDIODECODER_H
#define AUDIODECODER_H

// Qt
#include <QMap>

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

typedef enum TorcStreamTypes
{
    StreamTypeUnknown = -1,
    StreamTypeStart = 0,
    StreamTypeAudio = StreamTypeStart,
    StreamTypeVideo,
    StreamTypeSubtitle,
    StreamTypeRawText,
    StreamTypeAttachment,
    StreamTypeEnd
} TorcStreamTypes;

class TORC_AUDIO_PUBLIC AudioDecoder : public TorcDecoder
{
    friend class AudioDecoderFactory;
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
    TorcDecoder::DecoderState State     (void);
    void             Start              (void);
    void             Pause              (void);
    void             Stop               (void);
    void             Seek               (void);

  protected:
    explicit         AudioDecoder       (const QString &URI, TorcPlayer *Parent, int Flags);
    void             SetupAudio         (TorcAudioThread    *Thread);
    bool             OpenDemuxer        (TorcDemuxerThread  *Thread);
    void             CloseDemuxer       (TorcDemuxerThread  *Thread);
    void             DemuxPackets       (TorcDemuxerThread  *Thread);
    void             DecodeAudioFrames  (TorcAudioThread    *Thread);
    void             DecodeVideoFrames  (TorcVideoThread    *Thread);
    void             DecodeSubtitles    (TorcSubtitleThread *Thread);

    virtual bool     VideoBufferStatus  (int &Unused, int &Inuse, int &Held);
    virtual void     ProcessVideoPacket (AVStream* Stream, AVPacket *Packet);
    virtual void     SetupVideoDecoder  (AVStream* Stream);
    virtual void     CleanupVideoDecoder(AVStream* Stream);
    virtual void     FlushVideoBuffers  (void);

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
    AudioDecoderPriv                     *m_priv;
    bool                                 m_seek;
    double                               m_duration;
    int                                  m_bitrate;
    int                                  m_bitrateFactor;
    int                                  m_currentStreams[StreamTypeEnd];
    int                                  m_currentProgram;
    QList<TorcChapter*>                  m_chapters;
    QList<TorcProgramData*>              m_programs;
    QMap<QString,QString>                m_avMetaData;
};

#endif
