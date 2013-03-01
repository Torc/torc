#ifndef AUDIOWRAPPER_H
#define AUDIOWRAPPER_H

// Torc
#include "audiooutput.h"
#include "torcaudioexport.h"
#include "torcplayer.h"

class AudioOutput;

class TORC_AUDIO_PUBLIC AudioWrapper
{
    friend class AudioDecoder;

  public:
    AudioWrapper(TorcPlayer *Parent);
    ~AudioWrapper();

  public:
    // these are thread safe
    qint64       GetAudioTime      (quint64 &Age);
    void         SetAudioTime      (qint64 TimeStamp, quint64 Time);
    void         ClearAudioTime    (void);

  protected:
    void         Reset             (void);
    bool         Initialise        (void);
    void         SetAudioOutput    (AudioOutput *Output);
    void         SetAudioParams    (AudioFormat Format, int OriginalChannels,
                                    int Channels, int Codec, int Samplerate,
                                    bool Passthrough, int CodecProfile = -1);
    void         SetEffectiveDsp   (int Rate);
    void         CheckFormat       (void);
    void         SetNoAudio        (void);
    bool         HasAudioIn        (void) const;
    bool         HasAudioOut       (void) const;
    bool         ControlsVolume    (void) const;
    bool         Pause             (bool Pause);
    bool         IsPaused          (void);
    void         PauseAudioUntilBuffered (void);
    int          GetCodec          (void) const;
    int          GetNumChannels    (void) const;
    int          GetOriginalChannels (void) const;
    int          GetSampleRate     (void) const;
    uint         GetVolume         (void);
    uint         AdjustVolume      (int Change);
    uint         SetVolume         (int Volume);
    float        GetStretchFactor  (void) const;
    void         SetStretchFactor  (float Factor);
    bool         IsUpmixing        (void);
    bool         EnableUpmix       (bool Enable, bool Toggle = false);
    bool         CanUpmix          (void);
    void         SetPassthroughDisabled (bool Disabled);
    bool         ShouldPassthrough (int Samplerate, int Channels, int Codec, int Profile, bool UseProfile);
    bool         CanPassthrough    (int Samplerate, int Channels, int Codec, int Profile);
    bool         DecoderWillDownmix (int Codec);
    bool         CanDownmix        (void);
    bool         CanAC3            (void);
    bool         CanDTS            (void);
    bool         CanEAC3           (void);
    bool         CanTrueHD         (void);
    bool         CanDTSHD          (void);
    uint         GetMaxChannels    (void);
    int          GetMaxHDRate      (void);
    qint64       GetAudioTime      (void);
    bool         IsMuted           (void);
    bool         SetMuted          (bool mute);
    MuteState    GetMuteState      (void);
    MuteState    SetMuteState      (MuteState State);
    MuteState    IncrMuteState     (void);
    void         SetAudioOffset    (int Offset);
    void         AddAudioData      (char *Buffer, int Length, qint64 Timecode, int Frames);
    bool         NeedDecodingBeforePassthrough(void);
    qint64       LengthLastData    (void);
    int          GetFillStatus     (void);
    void         Drain             (void);

  private:
    void         DeleteOutput      (void);

  private:
    TorcPlayer  *m_parent;
    AudioOutput *m_audioOutput;
    int          m_audioOffset;
    QString      m_mainDevice;

    bool         m_passthrough;
    bool         m_passthroughDisabled;
    QString      m_passthroughDevice;

    int          m_channels;
    int          m_originalChannels;
    int          m_codec;
    AudioFormat  m_format;
    int          m_samplerate;
    int          m_codecProfile;
    float        m_stretchFactor;

    bool         m_noAudioIn;
    bool         m_noAudioOut;
    bool         m_controlsVolume;

    qint64       m_lastTimeStamp;
    quint64      m_lastTimeStampUpdate;
};

#endif // AUDIOWRAPPER_H
