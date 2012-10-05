#ifndef AUDIOSETTINGS_H
#define AUDIOSETTINGS_H

// Qt
#include <QString>

// Torc
#include "torcaudioexport.h"
#include "audiooutputsettings.h"

typedef enum
{
    AUDIOOUTPUT_UNKNOWN,
    AUDIOOUTPUT_VIDEO,
    AUDIOOUTPUT_MUSIC,
    AUDIOOUTPUT_TELEPHONY
} AudioOutputSource;

class TORC_AUDIO_PUBLIC AudioSettings
{
  public:
    AudioSettings();
    AudioSettings(const AudioSettings &Settings);
    AudioSettings(const QString       &MainDevice,
                  const QString       &PassthroughDevice,
                  AudioFormat          Format,
                  int                  Channels,
                  int                  Codec,
                  int                  Samplerate,
                  AudioOutputSource    Source,
                  bool                 SetInitialVolume,
                  bool                 UsePassthrough,
                  int                  UpmixerStartup = 0,
                  AudioOutputSettings *Custom = NULL);

    AudioSettings(AudioFormat Format,
                  int         Channels,
                  int         Codec,
                  int         Samplerate,
                  bool        UsePassthrough,
                  int         UpmixerStartup = 0,
                  int         CodecProfile = 0);

    AudioSettings(const QString &MainDevice, const QString &PassthroughDevice = QString());

    ~AudioSettings();
    void FixPassThrough(void);
    void TrimDeviceType(void);

    QString GetMainDevice(void) const;
    QString GetPassthroughDevice(void) const;

  public:
    QString             m_mainDevice;
    QString             m_passthroughDevice;
    AudioFormat         m_format;
    int                 m_channels;
    int                 m_codec;
    int                 m_codecProfile;
    int                 m_samplerate;
    bool                m_setInitialVolume;
    bool                m_usePassthrough;
    AudioOutputSource   m_sourceType;
    int                 m_upmixer;
    /**
     * If set to false, AudioOutput instance will not try to initially open
     * the audio device
     */
    bool                m_openOnInit;
    /**
     * m_custom contains a pointer to the audio device capabilities
     * if defined, AudioOutput will not try to automatically discover them.
     * This is used by the AudioTest setting screen where the user can
     * manually override and immediately use them.
     */
    AudioOutputSettings *m_custom;
};

#endif
