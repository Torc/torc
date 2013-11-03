#ifndef AUDIOOUTPUTOSX_H
#define AUDIOOUTPUTOSX_H

// Torc
#include "audiooutput.h"

#undef  AUDBUFSIZE
#define AUDBUFSIZE 512000

class AudioOutputOSXPriv;

class AudioOutputOSX : public AudioOutput
{
    friend class AudioOutputOSXPriv;

  public:
    AudioOutputOSX(const AudioSettings &Settings, AudioWrapper *Parent);
    virtual ~AudioOutputOSX();

    AudioOutputSettings* GetOutputSettings      (bool Digital);
    int64_t              GetAudiotime           (void);
    bool                 RenderAudio            (unsigned char *Buffer, int Size, unsigned long long Timestamp);
    int                  GetVolumeChannel       (int Channel) const;
    void                 SetVolumeChannel       (int Channel, int Volume);

  protected:
    bool                 OpenDevice             (void);
    void                 CloseDevice            (void);
    void                 WriteAudio             (unsigned char*, int);
    int                  GetBufferedOnSoundcard (void) const;
    bool                 StartOutputThread      (void);
    void                 StopOutputThread       (void);

  private:
    AudioOutputOSXPriv  *m_priv;
    int                  m_bufferedBytes;
};

#endif
