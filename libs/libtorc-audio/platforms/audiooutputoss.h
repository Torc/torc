#ifndef AUDIOOUTPUTOSS_H
#define AUDIOOUTPUTOSS_H

// Torc
#include "audiooutput.h"

class AudioOutputOSS : public AudioOutput
{
  public:
    AudioOutputOSS(const AudioSettings &settings, AudioWrapper *Parent);
    virtual ~AudioOutputOSS();

    int                  GetVolumeChannel       (int channel) const;
    void                 SetVolumeChannel       (int channel, int volume);

  protected:
    bool                 OpenDevice             (void);
    void                 CloseDevice            (void);
    void                 WriteAudio             (unsigned char *aubuf, int size);
    int                  GetBufferedOnSoundcard (void) const;
    AudioOutputSettings* GetOutputSettings      (bool digital);

  private:
    void                  VolumeInit            (void);
    void                  VolumeCleanup         (void);
    void                  SetFragSize           (void);

    int m_audioFd;
    int m_mixerFd;
    int m_control;
};

#endif

