#ifndef TORCDECODER_H
#define TORCDECODER_H

// Qt
#include <QMap>

// Torc
#include "torcaudioexport.h"
#include "torclanguage.h"

class TorcChapter;
class TorcStreamData;
class TorcProgramData;
class TorcDecoderPriv;
class TorcDecoderThread;
class TorcDemuxerThread;
class TorcVideoThread;
class TorcAudioThread;
class TorcSubtitleThread;

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

class TORC_AUDIO_PUBLIC TorcDecoder
{
    friend class TorcDemuxerThread;
    friend class TorcVideoThread;
    friend class TorcAudioThread;
    friend class TorcSubtitleThread;

  public:
    enum DecoderFlags
    {
        DecodeNone              = 0,
        DecodeAudio             = (1 << 0),
        DecodeVideo             = (1 << 1),
        DecodeForcedTracksOn    = (1 << 2),
        DecodeSubtitlesAlwaysOn = (1 << 3),
        DecodeMultithreaded     = (1 << 4)
    };

    typedef enum DecoderState
    {
        Errored = -1,
        None    = 0,
        Opening,
        Paused,
        Starting,
        Running,
        Pausing,
        Stopping,
        Stopped
    } DecoderState;

    static void      InitialiseLibav    (void);
    static QString   StreamTypeToString (TorcStreamTypes Type);
    static int       DecoderInterrupt   (void* Object);

  public:
    explicit         TorcDecoder        (const QString &URI, int Flags = DecodeNone);
    virtual         ~TorcDecoder        ();

    // Public API
    bool             Open               (void);
    DecoderState     State              (void);
    void             Start              (void);
    void             Pause              (void);
    void             Stop               (void);

  protected:
    bool             OpenDemuxer        (TorcDemuxerThread  *Thread);
    void             CloseDemuxer       (TorcDemuxerThread  *Thread);
    void             DemuxPackets       (TorcDemuxerThread  *Thread);
    void             DecodeVideoFrames  (TorcVideoThread    *Thread);
    void             DecodeAudioFrames  (TorcAudioThread    *Thread);
    void             DecodeSubtitles    (TorcSubtitleThread *Thread);

  private:
    void             TearDown           (void);
    void             SetFlag            (DecoderFlags Flag);
    bool             FlagIsSet          (DecoderFlags Flag);
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
    int                                  m_interruptDecoder;
    QString                              m_uri;
    int                                  m_flags;
    TorcDecoderPriv                     *m_priv;
    qint64                               m_seek;
    double                               m_duration;
    int                                  m_bitrate;
    int                                  m_bitrateFactor;
    int                                  m_currentStreams[StreamTypeEnd];
    int                                  m_currentProgram;
    QList<TorcChapter*>                  m_chapters;
    QList<TorcProgramData*>              m_programs;
    QMap<QString,QString>                m_avMetaData;
};

#endif // TORCDECODER_H
