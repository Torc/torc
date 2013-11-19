#ifndef TORCDECODER_H
#define TORCDECODER_H

// Qt
#include <QString>

// Torc
#include "torccoreexport.h"

class TorcPlayer;

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

class TORC_CORE_PUBLIC TorcDecoder
{
  public:
    typedef enum DecoderFlags
    {
        DecodeNone              = 0,
        DecodeAudio             = (1 << 0),
        DecodeVideo             = (1 << 1),
        DecodeForcedTracksOn    = (1 << 2),
        DecodeSubtitlesAlwaysOn = (1 << 3),
        DecodeMultithreaded     = (1 << 4)
    } DecoderFlags;

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

    typedef enum DemuxerState
    {
        DemuxerReady = 0,
        DemuxerWaiting,
        DemuxerFlush
    } DemuxerState;

    virtual ~TorcDecoder();
    static TorcDecoder*  Create           (int DecodeFlags, const QString &URI, TorcPlayer *Parent);
    virtual bool         HandleAction     (int Action) = 0;
    virtual bool         Open             (void) = 0;
    virtual DecoderState GetState         (void) = 0;
    virtual void         Start            (void) = 0;
    virtual void         Pause            (void) = 0;
    virtual void         Stop             (void) = 0;
    virtual void         Seek             (void) = 0;
    virtual int          GetCurrentStream (TorcStreamTypes Type) = 0;
    virtual int          GetStreamCount   (TorcStreamTypes Type) = 0;
};

class TORC_CORE_PUBLIC TorcDecoderFactory
{
  public:
    TorcDecoderFactory();
    virtual ~TorcDecoderFactory();
    static TorcDecoderFactory* GetTorcDecoderFactory (void);
    TorcDecoderFactory*        NextFactory           (void) const;
    virtual void               Score                 (int DecodeFlags, const QString &URI, int &Score, TorcPlayer *Parent) = 0;
    virtual TorcDecoder*       Create                (int DecodeFlags, const QString &URI, int &Score, TorcPlayer *Parent) = 0;

  protected:
    static TorcDecoderFactory* gTorcDecoderFactory;
    TorcDecoderFactory*        nextTorcDecoderFactory;
};

#endif // TORCDECODER_H
