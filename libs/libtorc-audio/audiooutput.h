#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

// Qt
#include <QMutex>
#include <QString>
#include <QVector>
#include <QWaitCondition>

// Torc
#include "torclocalcontext.h"
#include "torcthread.h"
#include "torcaudioexport.h"
#include "torccompat.h"
#include "samplerate/samplerate.h"
#include "audiosettings.h"
#include "audiooutputsettings.h"
#include "audiovolume.h"
#include "audiowrapper.h"
#include "audiooutputlisteners.h"

namespace soundtouch
{
    class SoundTouch;
}

class  AudioWrapper;
class  AudioSPDIFEncoder;
class  FreeSurround;
class  AudioOutputDigitalEncoder;
class  AsyncLooseLock;
struct AVCodecContext;

class TORC_AUDIO_PUBLIC AudioDeviceConfig
{
  public:
    AudioDeviceConfig(void);
    AudioDeviceConfig(const QString &Name, const QString &Description);

    QString             m_name;
    QString             m_description;
    AudioOutputSettings m_settings;
};

class TORC_AUDIO_PUBLIC AudioOutput : public AudioVolume, public AudioOutputListeners, public TorcThread
{
  public:
    enum
    {
        QUALITY_DISABLED = -1,
        QUALITY_LOW      =  0,
        QUALITY_MEDIUM   =  1,
        QUALITY_HIGH     =  2
    };

    static const uint kAudioSRCInputSize   = 16384;
    static const uint kAudioRingBufferSize = 3072000u;
    static QString QualityToString(int Quality);

  public:
    static QList<AudioDeviceConfig>  GetOutputList          (void);
    static AudioDeviceConfig*        GetAudioDeviceConfig   (const QString &Name, const QString &Description);
    static AudioOutput*              OpenAudio              (const QString &Name, AudioWrapper *Parent);
    static AudioOutput*              OpenAudio              (const AudioSettings &Settings, AudioWrapper *Parent);

  public:
    virtual ~AudioOutput();

    virtual void                     Reconfigure            (const AudioSettings &Settings);
    void                             SetSWVolume            (int   Volume, bool Save);
    int                              GetSWVolume            (void);
    void                             SetStretchFactor       (float Factor);
    float                            GetStretchFactor       (void) const;
    virtual int                      GetChannels            (void) const;
    virtual AudioFormat              GetFormat              (void) const;
    virtual int                      GetBytesPerFrame       (void) const;
    virtual AudioOutputSettings*     GetOutputSettingsCleaned (bool Digital = true);
    virtual AudioOutputSettings*     GetOutputSettingsUsers   (bool Digital = true);
    bool                             CanPassthrough         (int  Samplerate, int Channels, int Codec, int Profile) const;
    virtual bool                     CanDownmix             (void) const;
    void                             SetEffDsp              (int DSPRate); // DSPRate is 100 * samples/second
    virtual void                     Reset                  (void);
    bool                             AddFrames              (void *Buffer, int Frames, qint64 Timecode);
    bool                             AddData                (void *Buffer, int Length, qint64 Timecode, int Frames);
    virtual bool                     NeedDecodingBeforePassthrough (void) const;
    qint64                           LengthLastData         (void) const;
    void                             SetTimecode            (qint64 Timecode);
    bool                             IsPaused               (void) const;
    void                             Pause                  (bool Paused);
    void                             PauseUntilBuffered     (void);
    void                             Drain                  (void);
    qint64                           GetAudiotime           (void);
    virtual qint64                   GetAudioBufferedTime   (void);
    virtual void                     SetSourceBitrate       (int Rate);
    virtual int                      GetFillStatus          (void);
    virtual void                     GetBufferStatus        (uint &Fill, uint &Total);

    //  Only really used by the AudioOutputNULL object
    void                             BufferOutputData       (bool Buffer);
    virtual int                      ReadOutputData         (unsigned char *ReadBuffer, int MaxLength);
    bool                             IsUpmixing             (void);
    bool                             ToggleUpmix            (void);
    bool                             CanUpmix               (void);
    static bool                      IsPulseAudioRunning    (void);
    bool                             IsErrored              (void);

  protected:
    AudioOutput(const AudioSettings &Settings, AudioWrapper *Parent);

    void                             InitSettings           (const AudioSettings &Settings);
    void                             KillAudio              (void);
    void                             SetStretchFactorLocked (float Factor);
    int                              AudioFree              (void);
    int                              AudioReady             (void);
    inline int                       AudioLength            (void);
    int                              CheckFreeSpace         (int &Frames);
    int                              GetAudioData           (unsigned char *Buffer, int BufferSize, bool FullBuffer, volatile uint *LocalRaud = NULL);
    int                              GetBaseAudBufTimeCode  (void) const;
    virtual void                     run                    (void);

    virtual bool                     StartOutputThread      (void);
    virtual void                     StopOutputThread       (void);

    virtual bool                     OpenDevice             (void) = 0;
    virtual void                     CloseDevice            (void) = 0;
    virtual void                     WriteAudio             (unsigned char *Buffer, int Size) = 0;
    virtual int                      GetBufferedOnSoundcard (void) const = 0;

  private:
    bool                             SetupPassthrough       (int Codec, int CodecProfile, int &TempSamplerate, int &TempChannels);
    AudioOutputSettings*             OutputSettings         (bool Digital = true);
    int                              CopyWithUpmix          (char *Buffer, int Frames, uint &OriginalWaud);
    void                             SetAudiotime           (int  Frames, qint64 Timecode);

  protected:
    AudioWrapper                    *m_parent;
    bool                             m_configError;
    int                              m_channels;
    int                              m_codec;
    int                              m_bytesPerFrame;
    int                              m_outputBytesPerFrame;
    AudioFormat                      m_format;
    AudioFormat                      m_outputFormat;
    int                              m_samplerate;
    int                              m_effectiveDSPRate;
    int                              m_fragmentSize;
    long                             m_soundcardBufferSize;
    QString                          m_mainDevice;
    QString                          m_passthroughDevice;
    bool                             m_discreteDigital;
    bool                             m_passthrough;
    bool                             m_encode;
    bool                             m_reencode;
    float                            m_stretchFactor;
    int                              m_effectiveStretchFactor;
    AudioOutputSource                m_sourceType;
    bool                             m_killAudio;
    bool                             m_pauseAudio;
    bool                             m_actuallyPaused;
    bool                             m_wasPaused;
    bool                             m_unpauseWhenReady;
    bool                             m_setInitialVolume;
    bool                             m_bufferOutputDataForUse;
    int                              m_configuredChannels;
    int                              m_maxChannels;
    int                              m_srcQuality;

  private:
    AudioOutputSettings             *m_outputSettingsRaw;
    AudioOutputSettings             *m_outputSettings;
    AudioOutputSettings             *m_outputSettingsDigitaRaw;
    AudioOutputSettings             *m_outputSettingsDigital;
    bool                             m_needResampler;
    SRC_STATE                       *m_sourceState;
    soundtouch::SoundTouch          *m_soundStretch;
    AudioOutputDigitalEncoder       *m_digitalEncoder;
    FreeSurround                    *m_upmixer;
    int                              m_sourceChannels;
    int                              m_sourceSamplerate;
    int                              m_sourceBytePerFrame;
    bool                             m_upmixDefault;
    bool                             m_needsUpmix;
    bool                             m_needsDownmix;
    int                              m_surroundMode;
    float                            m_oldStretchFactor;
    int                              m_softwareVolume;
    QString                          m_softwareVolumeControl;
    bool                             m_processing;
    qint64                           m_framesBuffered;
    bool                             m_haveAudioThread;
    QMutex                           m_audioBufferLock;
    QMutex                           m_avSyncLock;
    qint64                           m_timecode;
    volatile uint                    m_raud;
    volatile uint                    m_waud;
    qint64                           m_audioBufferTimecode;
    AsyncLooseLock                  *m_resetActive;
    QMutex                           m_killAudioLock;
    long                             m_currentSeconds;
    long                             m_sourceBitrate;
    float                           *m_sourceInput;
    SRC_DATA                         m_sourceData;
    float                            m_sourceInputBuffer[kAudioSRCInputSize + 16];
    float                           *m_sourceOutput;
    int                              m_sourceOutputSize;
    unsigned char                    m_audioBuffer[kAudioRingBufferSize];
    uint                             m_configureSucceeded;
    qint64                           m_lengthLastData;
    AudioSPDIFEncoder               *m_spdifEnc;
    bool                             m_forcedProcessing;
    int                              m_previousBytesPerFrame;
};

#define AUDIO_PRIORITY_LOWEST 10
#define AUDIO_PRIORITY_LOW    20
#define AUDIO_PRIORITY_MEDIUM 30
#define AUDIO_PRIORITY_HIGH   40
#define AUDIO_PRIORITY_MATCH  100

class TORC_AUDIO_PUBLIC AudioFactory
{
  public:
    AudioFactory();
    virtual ~AudioFactory();
    static AudioFactory*   GetAudioFactory  (void);
    AudioFactory*          NextFactory      (void) const;
    virtual void           Score            (const AudioSettings &Settings, AudioWrapper *Parent, int &Score) = 0;
    virtual AudioOutput*   Create           (const AudioSettings &Settings, AudioWrapper *Parent, int &Score) = 0;
    virtual void           GetDevices       (QList<AudioDeviceConfig> &DeviceList) = 0;

  protected:
    static AudioFactory* gAudioFactory;
    AudioFactory*        nextAudioFactory;
};

#endif
