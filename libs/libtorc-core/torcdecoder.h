#ifndef TORCDECODER_H
#define TORCDECODER_H

// Qt
#include <QString>

// Torc
#include "torccoreexport.h"

class TorcPlayer;

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

    virtual ~TorcDecoder();
    static TorcDecoder*  Create       (int DecodeFlags, const QString &URI, TorcPlayer *Parent);
    virtual bool         HandleAction (int Action) = 0;
    virtual bool         Open         (void) = 0;
    virtual DecoderState State        (void) = 0;
    virtual void         Start        (void) = 0;
    virtual void         Pause        (void) = 0;
    virtual void         Stop         (void) = 0;
    virtual void         Seek         (void) = 0;
};

class TORC_CORE_PUBLIC DecoderFactory
{
  public:
    DecoderFactory();
    virtual ~DecoderFactory();
    static DecoderFactory* GetDecoderFactory (void);
    DecoderFactory*        NextFactory       (void) const;
    virtual TorcDecoder*   Create            (int DecodeFlags, const QString &URI, TorcPlayer *Parent) = 0;

  protected:
    static DecoderFactory* gDecoderFactory;
    DecoderFactory*        nextDecoderFactory;
};

#endif // TORCDECODER_H
