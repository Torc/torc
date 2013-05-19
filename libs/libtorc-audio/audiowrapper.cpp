// Qt
#include <QMutex>

// Torc
#include "torclocalcontext.h"
#include "torccoreutils.h"
#include "torcdecoder.h"
#include "audiooutpututil.h"
#include "audiowrapper.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

AudioWrapper::AudioWrapper(TorcPlayer *Parent)
  : m_parent(Parent),
    m_audioOutput(NULL),
    m_audioOffset(0),
    m_mainDevice(QString()),
    m_passthrough(false),
    m_passthroughDisabled(false),
    m_passthroughDevice(QString()),
    m_channels(-1),
    m_originalChannels(-1),
    m_codec(0),
    m_format(FORMAT_NONE),
    m_samplerate(44100),
    m_codecProfile(0),
    m_stretchFactor(1.0f),
    m_noAudioIn(false),
    m_noAudioOut(false),
    m_controlsVolume(true),
    m_lastTimeStamp(AV_NOPTS_VALUE),
    m_lastTimeStampUpdate(0)
{
    m_mainDevice        = gLocalContext->GetSetting(TORC_AUDIO + "AudioOutputDevice", QString(""));
    m_passthroughDevice = gLocalContext->GetSetting(TORC_AUDIO + "AudioPassthroughDevice", QString(""));
    m_controlsVolume    = gLocalContext->GetSetting(TORC_AUDIO + "ControlsVolume", true);
    m_samplerate        = gLocalContext->GetSetting(TORC_AUDIO + "AudioSamplerate", 44100);
    if (gLocalContext->GetSetting(TORC_AUDIO + "UseAudioPassthroughDevice", false))
        m_passthroughDevice = QString();
}

AudioWrapper::~AudioWrapper()
{
    DeleteOutput();
}

qint64 AudioWrapper::GetAudioTime(quint64 &Age)
{
    Age = m_lastTimeStampUpdate;
    return m_lastTimeStamp;
}

void AudioWrapper::SetAudioTime(qint64 TimeStamp, quint64 Time)
{
    m_lastTimeStamp       = TimeStamp;
    m_lastTimeStampUpdate = Time;
}

void AudioWrapper::ClearAudioTime(void)
{
    SetAudioTime(AV_NOPTS_VALUE, 0);
}

void AudioWrapper::Reset(void)
{
    if (m_audioOutput)
        m_audioOutput->Reset();
}

void AudioWrapper::DeleteOutput(void)
{
    ClearAudioTime();

    if (m_audioOutput)
    {
        delete m_audioOutput;
        m_audioOutput = NULL;
    }

    m_noAudioOut = true;
}

bool AudioWrapper::Initialise(void)
{
    bool audiomuted  = m_parent->GetPlayerFlags() & TorcPlayer::AudioMuted;
    bool audioneeded = m_parent->GetDecoderFlags() & TorcDecoder::DecodeAudio;
    //bool dummy = PlayerFlags  & TorcPlayer::AudioDummy;

    if ((m_format == FORMAT_NONE) || (m_channels <= 0) || (m_samplerate <= 0))
        m_noAudioIn = m_noAudioOut = true;
    else
        m_noAudioIn = false;

    if (audioneeded && !m_audioOutput)
    {
        AudioSettings aos = AudioSettings(m_mainDevice,
                                          m_passthroughDevice,
                                          m_format, m_channels,
                                          m_codec, m_samplerate,
                                          AUDIOOUTPUT_VIDEO,
                                          m_controlsVolume,
                                          m_passthrough);
        if (m_noAudioIn)
            aos.m_openOnInit = false;

        m_audioOutput = AudioOutput::OpenAudio(aos, this);

        if (!m_audioOutput)
        {
            LOG(VB_GENERAL, LOG_ERR, "Audio disabled (Failed to open audio device)");
            m_parent->SendUserMessage(QObject::tr("Failed to open audio device."));
            m_noAudioOut = true;
        }
        else
        {
            if (m_audioOutput->IsErrored())
            {
                LOG(VB_GENERAL, LOG_ERR, "Audio disabled");
                m_parent->SendUserMessage(QObject::tr("Audio disabled"));
                m_noAudioOut = true;
            }
            else
            {
                SetStretchFactor(m_stretchFactor);
                m_noAudioOut = false;
            }
        }
    }
    else if (!m_noAudioIn && m_audioOutput)
    {
        const AudioSettings settings(m_format, m_channels, m_codec,
                                     m_samplerate, m_passthrough, 0,
                                     m_codecProfile);
        m_audioOutput->Reconfigure(settings);

        if (m_audioOutput->IsErrored())
        {
            LOG(VB_GENERAL, LOG_ERR, "Audio disabled");
            m_parent->SendUserMessage(QObject::tr("Audio disabled"));
            m_noAudioOut = true;
        }
        else
        {
            m_noAudioOut = false;
            SetStretchFactor(m_stretchFactor);
        }
    }

    if (!m_noAudioOut && m_audioOutput)
    {
        LOG(VB_GENERAL, LOG_NOTICE, QString("Audio enabled (Muted: %1)").arg(audiomuted));
        if (audiomuted)
            SetMuteState(kMuteAll);
    }

    ClearAudioTime();

    return !m_noAudioOut;
}

void AudioWrapper::SetAudioOutput(AudioOutput *Output)
{
    DeleteOutput();
    m_audioOutput = Output;
}

void AudioWrapper::SetAudioParams(AudioFormat Format, int OriginalChannels,
                                  int Channels, int Codec, int Samplerate,
                                  bool Passthrough, int CodecProfile)
{
    m_format           = Format;
    m_originalChannels = OriginalChannels;
    m_channels         = Channels;
    m_codec            = Codec;
    m_samplerate       = Samplerate;
    m_passthrough      = Passthrough;
    m_codecProfile     = CodecProfile;
}

void AudioWrapper::SetEffectiveDsp(int Rate)
{
    if (!m_audioOutput || !m_noAudioOut)
        return;

    m_audioOutput->SetEffDsp(Rate);
}

void AudioWrapper::CheckFormat(void)
{
    if (m_format == FORMAT_NONE)
        m_noAudioIn = m_noAudioOut = true;
}

void AudioWrapper::SetNoAudio(void)
{
    m_noAudioOut = true;
}

bool AudioWrapper::HasAudioIn(void) const
{
    return !m_noAudioIn;
}

bool AudioWrapper::HasAudioOut(void) const
{
    return !m_noAudioOut;
}

bool AudioWrapper::ControlsVolume(void) const
{
    return m_controlsVolume;
}

bool AudioWrapper::Pause(bool Pause)
{
    if (!m_audioOutput || m_noAudioOut)
        return false;

    m_audioOutput->Pause(Pause);
    return true;
}

bool AudioWrapper::IsPaused(void)
{
    if (!m_audioOutput || m_noAudioOut)
        return false;

    return m_audioOutput->IsPaused();
}

void AudioWrapper::PauseAudioUntilBuffered(void)
{
    if (!m_audioOutput || m_noAudioOut)
        return;

    m_audioOutput->PauseUntilBuffered();
}

int AudioWrapper::GetCodec(void) const
{
    return m_codec;
}

int AudioWrapper::GetNumChannels(void) const
{
    return m_channels;
}

int AudioWrapper::GetOriginalChannels(void) const
{
    return m_originalChannels;
}

int AudioWrapper::GetSampleRate(void) const
{
    return m_samplerate;
}

uint AudioWrapper::GetVolume(void)
{
    if (!m_audioOutput || m_noAudioOut)
        return 0;

    return m_audioOutput->GetCurrentVolume();
}

uint AudioWrapper::AdjustVolume(int Change)
{
    if (!m_audioOutput || m_noAudioOut)
        return GetVolume();

    m_audioOutput->AdjustCurrentVolume(Change);
    return GetVolume();
}


uint AudioWrapper::SetVolume(int Volume)
{
    if (!m_audioOutput || m_noAudioOut)
        return GetVolume();

    m_audioOutput->SetCurrentVolume(Volume);
    return GetVolume();
}

float AudioWrapper::GetStretchFactor(void) const
{
    return m_stretchFactor;
}

void AudioWrapper::SetStretchFactor(float Factor)
{
    m_stretchFactor = Factor;
    if (!m_audioOutput)
        return;

    m_audioOutput->SetStretchFactor(m_stretchFactor);
}

bool AudioWrapper::IsUpmixing(void)
{
    if (!m_audioOutput)
        return false;

    return m_audioOutput->IsUpmixing();
}

bool AudioWrapper::EnableUpmix(bool Enable, bool Toggle)
{
    if (!m_audioOutput)
        return false;

    if (Toggle || (Enable != IsUpmixing()))
        return m_audioOutput->ToggleUpmix();
    return Enable;
}

bool AudioWrapper::CanUpmix(void)
{
    if (!m_audioOutput)
        return false;

    return m_audioOutput->CanUpmix();
}

bool AudioWrapper::CanDownmix(void)
{
    if (!m_audioOutput)
        return false;

    return m_audioOutput->CanDownmix();
}

void AudioWrapper::SetPassthroughDisabled(bool Disable)
{
    if (m_passthroughDisabled)
        return;

    if (Disable != m_passthroughDisabled)
    {
        m_passthroughDisabled = Disable;
        LOG(VB_GENERAL, LOG_INFO, QString("Passthrough %1")
            .arg(m_passthroughDisabled ? "disabled" : "enabled"));

        // FIXME this now needs to trigger TorcDecoder::SetupAudio
    }
}

bool AudioWrapper::ShouldPassthrough(int Samplerate, int Channels, int Codec,
                                     int Profile, bool UseProfile)
{
    if (m_passthroughDisabled)
        return false;

    if (!UseProfile && Codec == AV_CODEC_ID_DTS && !CanDTSHD())
        return CanPassthrough(Samplerate, Channels, Codec, FF_PROFILE_DTS);

    return CanPassthrough(Samplerate, Channels, Codec, Profile);
}

bool AudioWrapper::CanPassthrough(int Samplerate, int Channels, int Codec, int Profile)
{
    if (!m_audioOutput)
        return false;

    return m_audioOutput->CanPassthrough(Samplerate, Channels, Codec, Profile);
}

bool AudioWrapper::DecoderWillDownmix(int Codec)
{
    bool candownmix = CanDownmix();

    if (candownmix && AudioOutputUtil::HasHardwareFPU())
        return false;

    if (candownmix)
        return true;

    // use ffmpeg only for dolby codecs if we have to
    switch (Codec)
    {
        case AV_CODEC_ID_AC3:
        case AV_CODEC_ID_TRUEHD:
        case AV_CODEC_ID_EAC3:
            return true;
        default:
            break;
    }

    return false;
}

bool AudioWrapper::CanAC3(void)
{
    if (m_audioOutput)
        return m_audioOutput->GetOutputSettingsUsers(true)->CanFeature(FEATURE_AC3);

    return false;
}

bool AudioWrapper::CanEAC3(void)
{
    if (m_audioOutput)
        return m_audioOutput->GetOutputSettingsUsers(true)->CanFeature(FEATURE_EAC3);

    return false;
}

bool AudioWrapper::CanDTS(void)
{
    if (m_audioOutput)
        return m_audioOutput->GetOutputSettingsUsers(true)->CanFeature(FEATURE_DTS);

    return false;
}

bool AudioWrapper::CanDTSHD(void)
{
    if (m_audioOutput)
        return m_audioOutput->GetOutputSettingsUsers(true)->CanFeature(FEATURE_DTSHD);

    return false;
}

bool AudioWrapper::CanTrueHD(void)
{
    if (m_audioOutput)
        return m_audioOutput->GetOutputSettingsUsers(true)->CanFeature(FEATURE_TRUEHD);

    return false;
}

uint AudioWrapper::GetMaxChannels(void)
{
    if (!m_audioOutput)
        return 2;

    return m_audioOutput->GetOutputSettingsUsers(false)->BestSupportedChannels();
}

int AudioWrapper::GetMaxHDRate(void)
{
    if (!m_audioOutput)
        return 0;

    return m_audioOutput->GetOutputSettingsUsers(true)->GetMaxHDRate();
}

qint64 AudioWrapper::GetAudioTime(void)
{
    if (!m_audioOutput || m_noAudioOut)
        return 0LL;

    return m_audioOutput->GetAudiotime();
}

bool AudioWrapper::IsMuted(void)
{
    return GetMuteState() == kMuteAll;
}

MuteState AudioWrapper::GetMuteState(void)
{
    if (!m_audioOutput || m_noAudioOut)
        return kMuteAll;

    return m_audioOutput->GetMuteState();
}

MuteState AudioWrapper::SetMuteState(MuteState State)
{
    if (!m_audioOutput || m_noAudioOut)
        return kMuteAll;

    return m_audioOutput->SetMuteState(State);
}

MuteState AudioWrapper::IncrMuteState(void)
{
    if (!m_audioOutput || m_noAudioOut)
        return kMuteAll;

    return SetMuteState(AudioVolume::NextMuteState(GetMuteState()));
}

void AudioWrapper::SetAudioOffset(int Offset)
{
    m_audioOffset = Offset;
}

void AudioWrapper::AddAudioData(char *Buffer, int Length, qint64 Timecode, int Frames)
{
    if (!m_audioOutput || m_noAudioOut)
        return;

    if (Frames == 0 && Length > 0)
    {
        int samplesize = m_audioOutput->GetBytesPerFrame();
        if (samplesize <= 0)
            return;
        Frames = Length / samplesize;
    }

    if (!m_audioOutput->AddData(Buffer, Length, Timecode + m_audioOffset, Frames))
        LOG(VB_GENERAL, LOG_ERR, "Audio buffer overflow - data lost");
}

bool AudioWrapper::NeedDecodingBeforePassthrough(void)
{
    if (!m_audioOutput)
        return true;

    return m_audioOutput->NeedDecodingBeforePassthrough();
}

qint64 AudioWrapper::LengthLastData(void)
{
    if (!m_audioOutput)
        return 0;

    return m_audioOutput->LengthLastData();
}

int AudioWrapper::GetFillStatus(void)
{
    if (!m_audioOutput || m_noAudioOut)
        return -1;

    return m_audioOutput->GetFillStatus();
}

void AudioWrapper::Drain(void)
{
    if (m_audioOutput && !m_noAudioOut)
        m_audioOutput->Drain();
}
