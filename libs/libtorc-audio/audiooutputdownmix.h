#ifndef AUDIOOUTPUTDOWNMIX
#define AUDIOOUTPUTDOWNMIX

class AudioOutputDownmix
{
  public:
    static int DownmixFrames(int ChannelsIn, int ChannelsOut,
                             float *Dest, float *Source, int Frames);
};

#endif
