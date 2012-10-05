#ifndef AUDIOOUTPUTUTIL_H_
#define AUDIOOUTPUTUTIL_H_

// Torc
#include "audiooutputsettings.h"
#include "torcaudioexport.h"

class TORC_AUDIO_PUBLIC AudioOutputUtil
{
  public:
    static bool  HasHardwareFPU     (void);
    static int   ToFloat            (AudioFormat Format, void *Out, void *In, int Bytes);
    static int   FromFloat          (AudioFormat Format, void *Out, void *In, int Bytes);
    static void  MonoToStereo       (void *Dest,   void *Source, int Samples);
    static void  AdjustVolume       (void *Buffer, int Length,   int Volume,  bool  Music,  bool Upmix);
    static void  MuteChannel        (int   Bits,   int Channels, int Channel, void *Buffer, int  Bytes);
    static char* GeneratePinkFrames (char *Frames, int Channels, int Channel, int   Count,  int  Bits = 16);
};

#endif
