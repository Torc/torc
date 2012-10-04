#ifndef AUDIOVOLUME_H
#define AUDIOVOLUME_H

// Torc
#include "torcaudioexport.h"

typedef enum
{
    kMuteOff = 0,
    kMuteLeft,
    kMuteRight,
    kMuteAll
} MuteState;

class TORC_AUDIO_PUBLIC AudioVolume
{
  public:
    AudioVolume();
    virtual ~AudioVolume();

    void              SWVolume            (bool Set);
    bool              SWVolume            (void) const;
    virtual uint      GetCurrentVolume    (void) const;
    virtual void      SetCurrentVolume    (int Volume);
    virtual void      AdjustCurrentVolume (int Change);
    virtual void      ToggleMute          (void);
    virtual MuteState GetMuteState        (void) const;
    virtual MuteState SetMuteState        (MuteState State);
    static  MuteState NextMuteState       (MuteState State);

  protected:

    virtual int       GetVolumeChannel    (int Channel) const = 0;
    virtual void      SetVolumeChannel    (int Channel, int Volume) = 0;
    virtual void      SetSWVolume         (int Volume, bool Save) = 0;
    virtual int       GetSWVolume         (void) = 0;
    void              UpdateVolume        (void);
    void              SyncVolume          (void);
    void              SetChannels         (int Channels);

  protected:
    bool              m_internalVolumeControl;

 private:
    int               m_volume;
    MuteState         m_currentMuteState;
    bool              m_softwareVolume;
    bool              m_softwareVolumeSetting;
    int               m_channels;
};

#endif
