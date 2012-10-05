#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

// Qt
#include <QString>
#include <QVector>

// Torc
#include "torclocalcontext.h"
#include "torcaudioexport.h"
#include "torccompat.h"
#include "audiosettings.h"
#include "audiooutputsettings.h"
#include "audiovolume.h"
#include "audiooutputlisteners.h"

class TORC_AUDIO_PUBLIC AudioDeviceConfig
{
  public:
    AudioDeviceConfig(void);
    AudioDeviceConfig(const QString &Name, const QString &Description);

    QString             m_name;
    QString             m_description;
    AudioOutputSettings m_settings;
};

class TORC_AUDIO_PUBLIC AudioOutput : public AudioVolume, public AudioOutputListeners
{
  public:
    static void                     Cleanup              (void);
    static QList<AudioDeviceConfig> GetOutputList        (void);
    static AudioDeviceConfig*       GetAudioDeviceConfig (QString &Name,
                                                          QString &Description,
                                                          bool WillSuspendPulse = false);

    static AudioOutput *OpenAudio        (const QString &MainDevice,
                                          const QString &PassthroughDevice,
                                          AudioFormat Format,
                                          int Channels,
                                          int Codec,
                                          int Samplerate,
                                          AudioOutputSource Source,
                                          bool SetInitialVolume,
                                          bool Passthrough,
                                          int UpmixerStartup = 0,
                                          AudioOutputSettings *Custom = NULL);
    static AudioOutput *OpenAudio        (AudioSettings &Settings, bool WillSuspendPulse = true);
    static AudioOutput *OpenAudio        (const QString &MainDevice,
                                          const QString &PassthroughDevice = QString(),
                                          bool WillSuspendPulse = true);

    AudioOutput();
    virtual ~AudioOutput();

    virtual void   Reconfigure           (const AudioSettings &Settings) = 0;
    virtual void   SetStretchFactor      (float Factor);
    virtual float  GetStretchFactor      (void) const;
    virtual int    GetChannels           (void) const;
    virtual AudioFormat GetFormat        (void) const;
    virtual int    GetBytesPerFrame      (void) const;
    virtual AudioOutputSettings*
                GetOutputSettingsCleaned (bool Digital = true);
    virtual AudioOutputSettings*
                GetOutputSettingsUsers   (bool Digital = true);
    virtual bool    CanPassthrough       (int  Samplerate, int Channels, int Codec, int Profile) const;
    virtual bool    CanDownmix           (void) const;
    virtual void    SetEffDsp            (int DSPRate) = 0; // DSPRate is 100 * samples/second
    virtual void    Reset                (void) = 0;
    virtual bool    AddFrames            (void *Buffer, int Frames, qint64 Timecode) = 0;
    virtual bool    AddData              (void *Buffer, int Length, qint64 Timecode, int Frames) = 0;
    virtual bool    NeedDecodingBeforePassthrough (void) const;
    virtual qint64  LengthLastData       (void) const;
    virtual void    SetTimecode          (qint64 Timecode) = 0;
    virtual bool    IsPaused             (void) const = 0;
    virtual void    Pause                (bool Paused) = 0;
    virtual void    PauseUntilBuffered   (void) = 0;
    virtual void    Drain                (void) = 0;
    virtual qint64  GetAudiotime         (void) = 0;
    virtual qint64  GetAudioBufferedTime (void);
    virtual void    SetSourceBitrate     (int Rate);
    QString         GetError             (void) const;
    QString         GetWarning           (void) const;
    virtual void    GetBufferStatus      (uint &Fill, uint &Total);

    //  Only really used by the AudioOutputNULL object
    virtual void    BufferOutputData     (bool Buffer) = 0;
    virtual int     ReadOutputData       (unsigned char *ReadBuffer, int MaxLength) = 0;
    virtual bool    IsUpmixing           (void) = 0;
    virtual bool    ToggleUpmix          (void) = 0;
    virtual bool    CanUpmix             (void) = 0;
    static bool     IsPulseAudioRunning  (void);
    bool            PulseStatus          (void);

  protected:
    void            SilentError          (const QString &Message);
    void            Warn                 (const QString &Message);
    void            ClearError           (void);
    void            ClearWarning         (void);

    QString         m_lastError;
    QString         m_lastWarning;
    bool            m_pulseWasSuspended;
};

#endif
