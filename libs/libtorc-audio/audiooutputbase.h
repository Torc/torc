#ifndef AUDIOOUTPUTBASE_H
#define AUDIOOUTPUTBASE_H

// Qt
#include <QString>
#include <QMutex>
#include <QWaitCondition>

// Torc
#include "torcthread.h"
#include "torclogging.h"
#include "audiooutput.h"
#include "samplerate.h"

namespace soundtouch
{
    class SoundTouch;
}

class  AudioSPDIFEncoder;
class  FreeSurround;
class  AudioOutputDigitalEncoder;
class  AsyncLooseLock;
struct AVCodecContext;

class AudioOutputBase : public AudioOutput, public TorcThread
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

    AudioOutputBase(const AudioSettings &settings);
    virtual ~AudioOutputBase();

    AudioOutputSettings* GetOutputSettingsCleaned (bool  Digital = true);
    AudioOutputSettings* GetOutputSettingsUsers   (bool  Digital = false);
    virtual void         Reconfigure              (const AudioSettings &Settings);
    virtual void         SetEffDsp                (int   DSPRate);
    virtual void         SetStretchFactor         (float Factor);
    virtual float        GetStretchFactor         (void) const;
    virtual int          GetChannels              (void) const;
    virtual AudioFormat  GetFormat                (void) const;
    virtual int          GetBytesPerFrame         (void) const;
    virtual bool         CanPassthrough           (int   Samplerate, int Channels, int Codec, int Profile) const;
    virtual bool         CanDownmix               (void) const;
    virtual bool         IsUpmixing               (void);
    virtual bool         ToggleUpmix              (void);
    virtual bool         CanUpmix                 (void);
    virtual void         Reset                    (void);
    void                 SetSWVolume              (int   Volume, bool Save);
    int                  GetSWVolume              (void);
    virtual bool         AddFrames                (void *Buffer, int Frames, qint64 Timecode);
    virtual bool         AddData                  (void *Buffer, int Length, qint64 Timecode, int Frames);
    virtual bool         NeedDecodingBeforePassthrough (void) const;
    virtual qint64       LengthLastData           (void) const;
    virtual void         SetTimecode              (qint64 Timecode);
    virtual bool         IsPaused                 (void) const;
    virtual void         Pause                    (bool  Paused);
    void                 PauseUntilBuffered       (void);
    virtual void         Drain                    (void);
    virtual qint64       GetAudiotime             (void);
    virtual qint64       GetAudioBufferedTime     (void);
    virtual void         SetSourceBitrate         (int  Rate);
    virtual int          GetFillStatus            (void);
    virtual void         GetBufferStatus          (uint &Fill, uint &Total);
    virtual void         BufferOutputData         (bool Buffer);
    virtual int          ReadOutputData           (unsigned char *ReadBuffer, int MaxLength);

  protected:
    void                 InitSettings             (const AudioSettings &Settings);
    virtual bool         OpenDevice               (void) = 0;
    virtual void         CloseDevice              (void) = 0;
    virtual void         WriteAudio               (unsigned char *Buffer, int Size) = 0;
    virtual int          GetBufferedOnSoundcard   (void) const = 0;
    virtual AudioOutputSettings* GetOutputSettings(bool Digital);
    void                 KillAudio                (void);
    virtual bool         StartOutputThread        (void);
    virtual void         StopOutputThread         (void);
    int                  GetAudioData             (unsigned char *Buffer, int BufferSize, bool FullBuffer,
                                                   volatile uint *LocalRaud = NULL);
    virtual void         run                      (void);
    int                  CheckFreeSpace           (int  &Frames);
    inline int           audiolen                 (void);
    int                  audiofree                (void);
    int                  audioready               (void);
    void                 SetStretchFactorLocked   (float Factor);
    int                  GetBaseAudBufTimeCode    (void) const;

  private:
    bool                 SetupPassthrough         (int  Codec, int CodecProfile, int &TempSamplerate, int &TempChannels);
    AudioOutputSettings* OutputSettings           (bool Digital = true);
    int                  CopyWithUpmix            (char *Buffer, int Frames, uint &OriginalWaud);
    void                 SetAudiotime             (int  Frames, qint64 timecode);

  protected:
    int                        m_channels;
    int                        m_codec;
    int                        m_bytesPerFrame;
    int                        m_outputBytesPerFrame;
    AudioFormat                m_format;
    AudioFormat                m_outputFormat;
    int                        m_samplerate;
    int                        m_effectiveDSPRate;
    int                        m_fragmentSize;
    long                       m_soundcardBufferSize;
    QString                    m_mainDevice;
    QString                    m_passthroughDevice;
    bool                       m_discreteDigital;
    bool                       m_passthrough;
    bool                       m_encode;
    bool                       m_reencode;
    float                      m_stretchFactor;
    int                        m_effectiveStretchFactor;
    AudioOutputSource          m_sourceType;
    bool                       m_killAudio;
    bool                       m_pauseAudio;
    bool                       m_actuallyPaused;
    bool                       m_wasPaused;
    bool                       m_unpauseWhenReady;
    bool                       m_setInitialVolume;
    bool                       m_bufferOutputDataForUse;
    int                        m_configuredChannels;
    int                        m_maxChannels;
    int                        m_srcQuality;

  private:
    AudioOutputSettings       *m_outputSettingsRaw;
    AudioOutputSettings       *m_outputSettings;
    AudioOutputSettings       *m_outputSettingsDigitaRaw;
    AudioOutputSettings       *m_outputSettingsDigital;
    bool                       m_needResampler;
    SRC_STATE                 *m_sourceState;
    soundtouch::SoundTouch    *m_soundStretch;
    AudioOutputDigitalEncoder *m_digitalEncoder;
    FreeSurround              *m_upmixer;
    int                        m_sourceChannels;
    int                        m_sourceSamplerate;
    int                        m_sourceBytePerFrame;
    bool                       m_upmixDefault;
    bool                       m_needsUpmix;
    bool                       m_needsDownmix;
    int                        m_surroundMode;
    float                      m_oldStretchFactor;
    int                        m_softwareVolume;
    QString                    m_softwareVolumeControl;
    bool                       m_processing;
    qint64                     m_framesBuffered;
    bool                       m_haveAudioThread;
    QMutex                     m_audioBufferLock;
    QMutex                     m_avSyncLock;
    qint64                     m_timecode;
    volatile uint              m_raud;
    volatile uint              m_waud;
    qint64                     m_audioBufferTimecode;
    AsyncLooseLock            *m_resetActive;
    QMutex                     m_killAudioLock;
    long                       m_currentSeconds;
    long                       m_sourceBitrate;
    float                     *m_sourceInput;
    SRC_DATA                   m_sourceData;
    float                      m_sourceInputBuffer[kAudioSRCInputSize + 16];
    float                     *m_sourceOutput;
    int                        m_sourceOutputSize;
    unsigned char              m_audioBuffer[kAudioRingBufferSize];
    uint                       m_configureSucceeded;
    qint64                     m_lengthLastData;
    AudioSPDIFEncoder         *m_spdifEnc;
    bool                       m_forcedProcessing;
    int                        m_previousBytesPerFrame;
};

#endif
