#ifndef AUDIOOUTPUTALSA_H
#define AUDIOOUTPUTALSA_H

// Qt
#include <QMap>

// Tprc
#include "audiooutput.h"

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>

class AudioOutputALSA : public AudioOutput
{
  public:
    AudioOutputALSA(const AudioSettings &Settings);
    virtual ~AudioOutputALSA();

    // Volume control
    virtual int          GetVolumeChannel       (int Channel) const;
    virtual void         SetVolumeChannel       (int Channel, int Volume);
    static QMap<QString, QString> GetDevices    (const char* Type);

  protected:
    bool                 OpenDevice             (void);
    void                 CloseDevice            (void);
    void                 WriteAudio             (unsigned char *Buffer, int Size);
    int                  GetBufferedOnSoundcard (void) const;
    AudioOutputSettings* GetOutputSettings      (bool Passthrough);

  private:
    int                  TryOpenDevice          (int Mode, int TryAC3);
    int                  GetPCMInfo             (int &Card, int &Device, int &Subdevice);
    bool                 IncPreallocBufferSize  (int Requested, int BufferTime);
    inline int           SetParameters          (snd_pcm_t *Handle, snd_pcm_format_t Format,
                                                 uint Channels, uint Rate, uint BufferTime,
                                                 uint PeriodTime);
    QByteArray           GetELD                 (int Card, int Device, int Subdevice);
    bool                 OpenMixer              (void);

  private:
    snd_pcm_t           *m_pcmHandle;
    int                  m_preallocBufferSize;
    int                  m_card;
    int                  m_device;
    int                  m_subdevice;
    QString              m_lastdevice;

    struct
    {
        QString            device;
        QString            control;
        snd_mixer_t*       handle;
        snd_mixer_elem_t*  elem;
        long               volmin;
        long               volmax;
        long               volrange;
    } m_mixer;

};
#endif
