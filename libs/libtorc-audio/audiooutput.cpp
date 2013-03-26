/* class AudioOutput
*
* This file is part of the Torc project.
*
* Adapted from the AudioOutput class which is part of the MythTV project.
* Copyright various authors.
*/

// Qt
#include <QMutex>
#include <QDateTime>
#include <QProcess>

// Torc
#include "torcconfig.h"
#include "torccompat.h"
#include "torccoreutils.h"
#include "torcavutils.h"
#include "audiooutputdigitalencoder.h"
#include "audiooutpututil.h"
#include "audiooutputdownmix.h"
#include "SoundTouch.h"
#include "freesurround.h"
#include "audiospdifencoder.h"
#include "audiooutput.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

#define WPOS (m_audioBuffer + OriginalWaud)
#define RPOS (m_audioBuffer + m_raud)
#define ABUF (m_audioBuffer)
#define STST soundtouch::SAMPLETYPE
#define AOALIGN(x) (((long)&x + 15) & ~0xf);

// 1,2,5 and 7 channels are currently valid for upmixing if required
#define UPMIX_CHANNEL_MASK ((1<<1)|(1<<2)|(1<<5)|1<<7)
#define IS_VALID_UPMIX_CHANNEL(ch) ((1 << (ch)) & UPMIX_CHANNEL_MASK)

QString AudioOutput::QualityToString(int Quality)
{
    switch(Quality)
    {
        case QUALITY_DISABLED: return QString("disabled");
        case QUALITY_LOW:      return QString("low");
        case QUALITY_MEDIUM:   return QString("medium");
        case QUALITY_HIGH:     return QString("high");
        default:               return QString("unknown");
    }
}

class AsyncLooseLock
{
  public:
    AsyncLooseLock();
    void Clear        (void);
    void Ref          (void);
    bool TestAndDeref (void);

  private:
    int m_head;
    int m_tail;
};

AsyncLooseLock::AsyncLooseLock()
  : m_head(0),
    m_tail(0)
{
}

void AsyncLooseLock::Clear(void)
{
    m_head = m_tail = 0;
}

void AsyncLooseLock::Ref(void)
{
    m_head++;
}

bool AsyncLooseLock::TestAndDeref(void)
{
    bool r;
    if ((r = (m_head != m_tail)))
        m_tail++;
    return r;
}

AudioDeviceConfig::AudioDeviceConfig()
  : m_settings(AudioOutputSettings(true))
{
}

AudioDeviceConfig::AudioDeviceConfig(const QString &Name, const QString &Description)
  : m_name(Name),
    m_description(Description),
    m_settings(AudioOutputSettings(true))
{
}

AudioOutput* AudioOutput::OpenAudio(const QString &Name, AudioWrapper *Parent)
{
    AudioSettings settings(Name);
    return OpenAudio(settings, Parent);
}

AudioOutput *AudioOutput::OpenAudio(const AudioSettings &Settings, AudioWrapper *Parent)
{
    AudioOutput *output = NULL;

    int score = 0;
    AudioFactory* factory = AudioFactory::GetAudioFactory();
    for ( ; factory; factory = factory->NextFactory())
        (void)factory->Score(Settings, Parent, score);

    factory = AudioFactory::GetAudioFactory();
    for ( ; factory; factory = factory->NextFactory())
    {
        output = factory->Create(Settings, Parent, score);
        if (output)
            break;
    }

    if (!output)
        LOG(VB_GENERAL, LOG_ERR, "Failed to create audio output");

    return output;
}

QList<AudioDeviceConfig> AudioOutput::GetOutputList(void)
{
    QList<AudioDeviceConfig> list;

    AudioFactory* factory = AudioFactory::GetAudioFactory();
    for ( ; factory; factory = factory->NextFactory())
        factory->GetDevices(list);

    return list;
}

AudioOutput::AudioOutput(const AudioSettings &Settings, AudioWrapper *Parent)
  : AudioVolume(),
    AudioOutputListeners(),
    TorcThread("Audio"),
    m_parent(Parent),
    m_configError(false),
    m_channels(-1),
    m_codec(CODEC_ID_NONE),
    m_bytesPerFrame(0),
    m_outputBytesPerFrame(0),
    m_format(FORMAT_NONE),
    m_outputFormat(FORMAT_NONE),
    m_samplerate(-1),
    m_effectiveDSPRate(0),
    m_fragmentSize(0),
    m_soundcardBufferSize(0),
    m_mainDevice(Settings.GetMainDevice()),
    m_passthroughDevice(Settings.GetPassthroughDevice()),
    m_discreteDigital(false),
    m_passthrough(false),
    m_encode(false),
    m_reencode(false),
    m_stretchFactor(1.0f),
    m_effectiveStretchFactor(100000),
    m_sourceType(Settings.m_sourceType),
    m_killAudio(false),
    m_pauseAudio(false),
    m_actuallyPaused(false),
    m_wasPaused(false),
    m_unpauseWhenReady(false),
    m_setInitialVolume(Settings.m_setInitialVolume),
    m_bufferOutputDataForUse(false),
    m_configuredChannels(0),
    m_maxChannels(0),
    m_srcQuality(QUALITY_MEDIUM),
    m_outputSettingsRaw(NULL),
    m_outputSettings(NULL),
    m_outputSettingsDigitaRaw(NULL),
    m_outputSettingsDigital(NULL),
    m_needResampler(false),
    m_sourceState(NULL),
    m_soundStretch(NULL),
    m_digitalEncoder(NULL),
    m_upmixer(NULL),
    m_sourceChannels(-1),
    m_sourceSamplerate(0),
    m_sourceBytePerFrame(0),
    m_upmixDefault(false),
    m_needsUpmix(false),
    m_needsDownmix(false),
    m_surroundMode(QUALITY_LOW),
    m_oldStretchFactor(1.0f),
    m_softwareVolume(80),
    m_softwareVolumeControl(QString()),
    m_processing(false),
    m_framesBuffered(0),
    m_haveAudioThread(false),
    m_timecode(0),
    m_raud(0),
    m_waud(0),
    m_audioBufferTimecode(0),
    m_resetActive(new AsyncLooseLock()),
    m_killAudioLock(QMutex::NonRecursive),
    m_currentSeconds(-1),
    m_sourceBitrate(-1),
    m_sourceOutput(NULL),
    m_sourceOutputSize(0),
    m_configureSucceeded(false),
    m_lengthLastData(0),
    m_spdifEnc(NULL),
    m_forcedProcessing(false)
{
    m_sourceInput = (float *)AOALIGN(m_sourceInputBuffer);
    memset(&m_sourceData,   0, sizeof(SRC_DATA));
    memset(m_sourceInputBuffer,  0, sizeof(m_sourceInputBuffer));
    memset(m_audioBuffer, 0, sizeof(m_audioBuffer));

    if (gLocalContext->GetSetting(TORC_AUDIO + "SRCQualityOverride", false))
    {
        m_srcQuality = gLocalContext->GetSetting(TORC_AUDIO + "SRCQuality", QUALITY_MEDIUM);
        LOG(VB_AUDIO, LOG_INFO,  QString("SRC quality: %1").arg(QualityToString(m_srcQuality)));
    }
}

void AudioOutput::InitSettings(const AudioSettings &Settings)
{
    if (Settings.m_custom)
    {
        m_outputSettings = new AudioOutputSettings;
        *m_outputSettings = *Settings.m_custom;
        m_outputSettingsDigital = m_outputSettings;
        m_maxChannels = m_outputSettings->BestSupportedChannels();
        m_configuredChannels = m_maxChannels;
        return;
    }

    // Ask the subclass what we can send to the device
    m_outputSettings = GetOutputSettingsUsers(false);
    m_outputSettingsDigital = GetOutputSettingsUsers(true);

    m_maxChannels = std::max(m_outputSettings->BestSupportedChannels(),
                            m_outputSettingsDigital->BestSupportedChannels());
    m_configuredChannels = m_maxChannels;

    m_upmixDefault = m_maxChannels > 2 ?
        gLocalContext->GetSetting(TORC_AUDIO + "AudioDefaultUpmix", false) : false;

    if (Settings.m_upmixer == 1) // music, upmixer off
        m_upmixDefault = false;
    else if (Settings.m_upmixer == 2) // music, upmixer on
        m_upmixDefault = true;
}

AudioOutput::~AudioOutput()
{
    if (!m_killAudio)
        LOG(VB_GENERAL, LOG_ERR, "Programmer Error: ~AudioOutput called, but KillAudio has not been called!");

    // We got this from a subclass, delete it
    delete m_outputSettings;
    delete m_outputSettingsRaw;

    if (m_outputSettings != m_outputSettingsDigital)
    {
        delete m_outputSettingsDigital;
        delete m_outputSettingsDigitaRaw;
    }

    if (m_sourceOutputSize > 0)
        delete[] m_sourceOutput;

    delete m_resetActive;
}

void AudioOutput::Reconfigure(const AudioSettings &Settings)
{
    AudioSettings settings    = Settings;
    int  lsource_channels     = settings.m_channels;
    int  lconfigured_channels = m_configuredChannels;
    bool lneeds_upmix         = false;
    bool lneeds_downmix       = false;
    bool lreenc               = false;
    bool lenc                 = false;

    if (!settings.m_usePassthrough)
    {
        // Do we upmix stereo or mono?
        lconfigured_channels =
            (m_upmixDefault && lsource_channels <= 2) ? 6 : lsource_channels;
        bool cando_channels =
            m_outputSettings->IsSupportedChannels(lconfigured_channels);

        // check if the number of channels could be transmitted via AC3 encoding
        lenc = m_outputSettingsDigital->CanFeature(FEATURE_AC3) &&
            (!m_outputSettings->CanFeature(FEATURE_LPCM) &&
             lconfigured_channels > 2 && lconfigured_channels <= 6);

        if (!lenc && !cando_channels)
        {
            // if hardware doesn't support source audio configuration
            // we will upmix/downmix to what we can
            // (can safely assume hardware supports stereo)
            switch (lconfigured_channels)
            {
                case 7:
                    lconfigured_channels = 8;
                    break;
                case 8:
                case 5:
                    lconfigured_channels = 6;
                        break;
                case 6:
                case 4:
                case 3:
                case 2: //Will never happen
                    lconfigured_channels = 2;
                    break;
                case 1:
                    lconfigured_channels = m_upmixDefault ? 6 : 2;
                    break;
                default:
                    lconfigured_channels = 2;
                    break;
            }
        }
        // Make sure we never attempt to output more than what we can
        // the upmixer can only upmix to 6 channels when source < 6
        if (lsource_channels <= 6)
            lconfigured_channels = std::min(lconfigured_channels, 6);
        lconfigured_channels = std::min(lconfigured_channels, m_maxChannels);
        /* Encode to AC-3 if we're allowed to passthrough but aren't currently
           and we have more than 2 channels but multichannel PCM is not
           supported or if the device just doesn't support the number of
           channels */
        lenc = m_outputSettingsDigital->CanFeature(FEATURE_AC3) &&
            ((!m_outputSettings->CanFeature(FEATURE_LPCM) &&
              lconfigured_channels > 2) ||
             !m_outputSettings->IsSupportedChannels(lconfigured_channels));

        /* Might we reencode a bitstream that's been decoded for timestretch?
           If the device doesn't support the number of channels - see below */
        if (m_outputSettingsDigital->CanFeature(FEATURE_AC3) &&
            (settings.m_codec == CODEC_ID_AC3 || settings.m_codec == CODEC_ID_DTS))
        {
            lreenc = true;
        }

        // Enough channels? Upmix if not, but only from mono/stereo/5.0 to 5.1
        if (IS_VALID_UPMIX_CHANNEL(settings.m_channels) && settings.m_channels < lconfigured_channels)
        {
            LOG(VB_AUDIO, LOG_INFO,  QString("Needs upmix from %1 -> %2 channels")
                    .arg(settings.m_channels).arg(lconfigured_channels));
            settings.m_channels = lconfigured_channels;
            lneeds_upmix = true;
        }
        else if (settings.m_channels > lconfigured_channels)
        {
            LOG(VB_AUDIO, LOG_INFO,  QString("Needs downmix from %1 -> %2 channels")
                    .arg(settings.m_channels).arg(lconfigured_channels));
            settings.m_channels = lconfigured_channels;
            lneeds_downmix = true;
        }
    }

    m_configError = false;
    bool general_deps = true;

    /* Set TempSamplerate and TempChannels to appropriate values
       if passing through */
    int TempSamplerate, TempChannels;
    if (settings.m_usePassthrough)
    {
        TempSamplerate = settings.m_samplerate;
        SetupPassthrough(settings.m_codec, settings.m_codecProfile,
                         TempSamplerate, TempChannels);
        general_deps = m_samplerate == TempSamplerate && m_channels == TempChannels;
    }

    // Check if anything has changed
    general_deps &=
        settings.m_format == m_format &&
        settings.m_samplerate  == m_sourceSamplerate &&
        settings.m_usePassthrough == m_passthrough &&
        lconfigured_channels == m_configuredChannels &&
        lneeds_upmix == m_needsUpmix && lreenc == m_reencode &&
        lsource_channels == m_sourceChannels &&
        lneeds_downmix == m_needsDownmix;

    if (general_deps && m_configureSucceeded)
    {
        LOG(VB_AUDIO, LOG_INFO,  "Reconfigure(): No change -> exiting");
        return;
    }

    KillAudio();

    QMutexLocker lock(&m_audioBufferLock);
    QMutexLocker lockav(&m_avSyncLock);

    m_waud = m_raud = 0;
    m_resetActive->Clear();
    m_actuallyPaused = m_processing = m_forcedProcessing = false;

    m_channels             = settings.m_channels;
    m_sourceChannels       = lsource_channels;
    m_reencode             = lreenc;
    m_codec                = settings.m_codec;
    m_passthrough          = settings.m_usePassthrough;
    m_configuredChannels   = lconfigured_channels;
    m_needsUpmix           = lneeds_upmix;
    m_needsDownmix         = lneeds_downmix;
    m_format               = m_outputFormat = settings.m_format;
    m_sourceSamplerate     = m_samplerate   = settings.m_samplerate;
    m_encode               = lenc;

    m_killAudio = m_pauseAudio = false;
    m_wasPaused = true;

    // Don't try to do anything if audio hasn't been initialized yet
    if (m_sourceChannels <= 0 || m_format <= 0 || m_samplerate <= 0)
    {
        m_configError = true;
        LOG(VB_GENERAL, LOG_ERR, QString("Invalid audio parameters ch %1 fmt %2 @ %3Hz")
            .arg(m_sourceChannels).arg(m_format).arg(m_samplerate));
        return;
    }

    LOG(VB_AUDIO, LOG_INFO,  QString("Original codec was %1, %2, %3 kHz, %4 channels")
            .arg(AVCodecToString(m_codec))
            .arg(m_outputSettings->FormatToString(m_format))
            .arg(m_samplerate / 1000)
            .arg(m_sourceChannels));

    if (m_needsDownmix && m_sourceChannels > 8)
    {
        LOG(VB_GENERAL, LOG_ERR, "Aborting Audio Reconfigure. "
              "Can't handle audio with more than 8 channels.");
        return;
    }

    LOG(VB_AUDIO, LOG_INFO,  QString("enc(%1), passthrough(%2), features (%3) "
                    "configured_channels(%4), %5 channels supported(%6) "
                    "max_channels(%7)")
            .arg(m_encode)
            .arg(m_passthrough)
            .arg(m_outputSettingsDigital->FeaturesToString())
            .arg(m_configuredChannels)
            .arg(m_channels)
            .arg(OutputSettings(m_encode || m_passthrough)->IsSupportedChannels(m_channels))
            .arg(m_maxChannels));

    int dest_rate = 0;

    // Force resampling if we are encoding to AC3 and sr > 48k
    // or if 48k override was checked in settings
    if ((m_samplerate != 48000 &&
         gLocalContext->GetSetting(TORC_AUDIO + "Audio48kOverride", false)) ||
         (m_encode && (m_samplerate > 48000)))
    {
        LOG(VB_AUDIO, LOG_INFO,  "Forcing resample to 48 kHz");
        if (m_srcQuality < 0)
            m_srcQuality = QUALITY_MEDIUM;
        m_needResampler = true;
        dest_rate = 48000;
    }
    // this will always be false for passthrough audio as
    // CanPassthrough() already tested these conditions
    else if ((m_needResampler =
              !OutputSettings(m_encode || m_passthrough)->IsSupportedRate(m_samplerate)))
    {
        dest_rate = OutputSettings(m_encode)->NearestSupportedRate(m_samplerate);
    }

    if (m_needResampler && m_srcQuality > QUALITY_DISABLED)
    {
        int error;
        m_samplerate = dest_rate;

        LOG(VB_GENERAL, LOG_INFO,   QString("Resampling from %1 kHz to %2 kHz with quality %3")
                .arg(settings.m_samplerate / 1000).arg(m_samplerate / 1000)
                .arg(QualityToString(m_srcQuality)));

        int chans = m_needsDownmix ? m_configuredChannels : m_sourceChannels;

        m_sourceState = src_new(2 - m_srcQuality, chans, &error);
        if (error)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Error creating resampler: %1")
                  .arg(src_strerror(error)));
            m_sourceState = NULL;
            return;
        }

        m_sourceData.src_ratio = (double)m_samplerate / settings.m_samplerate;
        m_sourceData.data_in   = m_sourceInput;
        int newsize        = (int)(kAudioSRCInputSize * m_sourceData.src_ratio + 15)
                             & ~0xf;

        if (m_sourceOutputSize < newsize)
        {
            m_sourceOutputSize = newsize;
            LOG(VB_AUDIO, LOG_INFO,  QString("Resampler allocating %1").arg(newsize));
            if (m_sourceOutput)
                delete [] m_sourceOutput;
            m_sourceOutput = new float[m_sourceOutputSize];
        }
        m_sourceData.data_out       = m_sourceOutput;
        m_sourceData.output_frames  = m_sourceOutputSize / chans;
        m_sourceData.end_of_input = 0;
    }

    if (m_encode)
    {
        if (m_reencode)
            LOG(VB_AUDIO, LOG_INFO,  "Reencoding decoded AC-3/DTS to AC-3");

        LOG(VB_AUDIO, LOG_INFO,  QString("Creating AC-3 Encoder with sr = %1, ch = %2")
                .arg(m_samplerate).arg(m_configuredChannels));

        m_digitalEncoder = new AudioOutputDigitalEncoder();
        if (!m_digitalEncoder->Init(CODEC_ID_AC3, 448000, m_samplerate,
                           m_configuredChannels))
        {
            LOG(VB_GENERAL, LOG_ERR, "AC-3 encoder initialization failed");
            delete m_digitalEncoder;
            m_digitalEncoder = NULL;
            m_encode = false;
            // upmixing will fail if we needed the encoder
            m_needsUpmix = false;
        }
    }

    if (m_passthrough)
    {
        //AC3, DTS, DTS-HD MA and TrueHD all use 16 bits samples
        m_channels = TempChannels;
        m_samplerate = TempSamplerate;
        m_format = m_outputFormat = FORMAT_S16;
        m_sourceBytePerFrame = m_channels * m_outputSettings->SampleSize(m_format);
    }
    else
    {
        m_sourceBytePerFrame = m_sourceChannels * m_outputSettings->SampleSize(m_format);
    }

    // Turn on float conversion?
    if (m_needResampler || m_needsUpmix || m_needsDownmix ||
        m_stretchFactor != 1.0f || (m_internalVolumeControl && SWVolume()) ||
        (m_encode && m_outputFormat != FORMAT_S16) ||
        !OutputSettings(m_encode || m_passthrough)->IsSupportedFormat(m_outputFormat))
    {
        LOG(VB_AUDIO, LOG_INFO,  "Audio processing enabled");
        m_processing  = true;
        if (m_encode)
            m_outputFormat = FORMAT_S16;  // Output s16le for AC-3 encoder
        else
            m_outputFormat = m_outputSettings->BestSupportedFormat();
    }

    m_bytesPerFrame =  m_processing ? sizeof(float) : m_outputSettings->SampleSize(m_format);
    m_bytesPerFrame *= m_channels;

    if (m_encode)
        m_channels = 2; // But only post-encoder

    m_outputBytesPerFrame = m_channels * m_outputSettings->SampleSize(m_outputFormat);

    LOG(VB_GENERAL, LOG_INFO,
        QString("Opening audio device '%1' ch %2(%3) sr %4 sf %5 reenc %6")
        .arg(m_mainDevice).arg(m_channels).arg(m_sourceChannels).arg(m_samplerate)
        .arg(m_outputSettings->FormatToString(m_outputFormat)).arg(m_reencode));

    m_audioBufferTimecode = m_timecode = m_framesBuffered = 0;
    m_currentSeconds = m_sourceBitrate = -1;
    m_effectiveDSPRate = m_samplerate * 100;

    // Actually do the device specific open call
    if (!OpenDevice())
    {
        LOG(VB_GENERAL, LOG_ERR, "Aborting reconfigure");
        m_configureSucceeded = false;
        return;
    }

    LOG(VB_AUDIO, LOG_INFO,  QString("Audio fragment size: %1").arg(m_fragmentSize));

    // Only used for software volume
    if (m_setInitialVolume && m_internalVolumeControl && SWVolume())
    {
        LOG(VB_AUDIO, LOG_INFO,  "Software volume enabled");
        m_softwareVolumeControl  = gLocalContext->GetSetting(TORC_AUDIO + "MixerControl", QString("PCM"));
        m_softwareVolumeControl += "MixerVolume";
        m_softwareVolume = gLocalContext->GetSetting(m_softwareVolumeControl, 80);
    }

    AudioVolume::SetChannels(m_configuredChannels);
    AudioVolume::SyncVolume();
    AudioVolume::UpdateVolume();

    if (m_needsUpmix && IS_VALID_UPMIX_CHANNEL(m_sourceChannels) &&
        m_configuredChannels > 2)
    {
        m_surroundMode = gLocalContext->GetSetting(TORC_AUDIO + "AudioUpmixType", QUALITY_HIGH);
        if ((m_upmixer = new FreeSurround(m_samplerate, m_sourceType== AUDIOOUTPUT_VIDEO,
                                    (FreeSurround::SurroundMode)m_surroundMode)))
            LOG(VB_AUDIO, LOG_INFO,  QString("Create %1 quality upmixer done")
                    .arg(QualityToString(m_surroundMode)));
        else
        {
            LOG(VB_GENERAL, LOG_ERR,   "Failed to create upmixer");
            m_needsUpmix = false;
        }
    }

    LOG(VB_AUDIO, LOG_INFO,  QString("Audio Stretch Factor: %1").arg(m_stretchFactor));
    SetStretchFactorLocked(m_oldStretchFactor);

    // Setup visualisations, zero the visualisations buffers
    PrepareListeners();

    if (m_unpauseWhenReady)
        m_pauseAudio = m_actuallyPaused = true;

    m_configureSucceeded = true;

    StartOutputThread();

    LOG(VB_AUDIO, LOG_INFO,  "Ending Reconfigure()");
}

void AudioOutput::SetSWVolume(int Volume, bool Save)
{
    m_softwareVolume = Volume;
    if (Save && !m_softwareVolumeControl.isEmpty())
        gLocalContext->SetSetting(m_softwareVolumeControl, m_softwareVolume);
}

int AudioOutput::GetSWVolume(void)
{
    return m_softwareVolume;
}

void AudioOutput::SetStretchFactor(float Factor)
{
    QMutexLocker lock(&m_audioBufferLock);
    SetStretchFactorLocked(Factor);
}

float AudioOutput::GetStretchFactor(void) const
{
    return m_stretchFactor;
}

int AudioOutput::GetChannels(void) const
{
    return m_channels;
}

AudioFormat AudioOutput::GetFormat(void) const
{
    return m_format;
}

int AudioOutput::GetBytesPerFrame(void) const
{
    return m_sourceBytePerFrame;
}

/**
 * Returns capabilities supported by the audio device
 * amended to take into account the digital audio
 * options (AC3, DTS, E-AC3 and TrueHD)
 */
AudioOutputSettings* AudioOutput::GetOutputSettingsCleaned(bool Digital)
{
    // If we've already checked the port, use the cached version instead
    if (!m_discreteDigital || !Digital)
    {
        Digital = false;
        if (m_outputSettingsRaw)
            return m_outputSettingsRaw;
    }
    else if (m_outputSettingsDigitaRaw)
        return m_outputSettingsDigitaRaw;

    AudioOutputSettings* aosettings = new AudioOutputSettings();
    if (aosettings)
        aosettings->GetCleaned();
    else
        aosettings = new AudioOutputSettings(true);

    if (Digital)
        return (m_outputSettingsDigitaRaw = aosettings);
    else
        return (m_outputSettingsRaw = aosettings);
}

/**
 * Returns capabilities supported by the audio device
 * amended to take into account the digital audio
 * options (AC3, DTS, E-AC3 and TrueHD) as well as the user settings
 */
AudioOutputSettings* AudioOutput::GetOutputSettingsUsers(bool Digital)
{
    if (!m_discreteDigital || !Digital)
    {
        Digital = false;
        if (m_outputSettings)
            return m_outputSettings;
    }
    else if (m_outputSettingsDigital)
        return m_outputSettingsDigital;

    AudioOutputSettings* aosettings = new AudioOutputSettings;

    *aosettings = *GetOutputSettingsCleaned(Digital);
    aosettings->GetUsers();

    if (Digital)
        return (m_outputSettingsDigital = aosettings);
    else
        return (m_outputSettings = aosettings);
}

/**
 * Test if we can output digital audio and if sample rate is supported
 */
bool AudioOutput::CanPassthrough(int Samplerate, int Channels, int Codec, int Profile) const
{
    DigitalFeature arg = FEATURE_NONE;
    bool           ret = !(m_internalVolumeControl && SWVolume());

    switch (Codec)
    {
        case CODEC_ID_AC3:
            arg = FEATURE_AC3;
            break;
        case CODEC_ID_DTS:
            switch (Profile)
            {
                case FF_PROFILE_DTS:
                case FF_PROFILE_DTS_ES:
                case FF_PROFILE_DTS_96_24:
                    arg = FEATURE_DTS;
                    break;
                case FF_PROFILE_DTS_HD_HRA:
                case FF_PROFILE_DTS_HD_MA:
                    arg = FEATURE_DTSHD;
                    break;
                default:
                    break;
            }
            break;
        case CODEC_ID_EAC3:
            arg = FEATURE_EAC3;
            break;
        case CODEC_ID_TRUEHD:
            arg = FEATURE_TRUEHD;
            break;
    }
    // we can't passthrough any other codecs than those defined above
    ret &= m_outputSettingsDigital->CanFeature(arg);
    ret &= m_outputSettingsDigital->IsSupportedFormat(FORMAT_S16);
    ret &= m_outputSettingsDigital->IsSupportedRate(Samplerate);
    // if we must resample to 48kHz ; we can't passthrough
    ret &= !((Samplerate != 48000) &&
             gLocalContext->GetSetting(TORC_AUDIO + "Audio48kOverride", false));
    // Don't know any cards that support spdif clocked at < 44100
    // Some US cable transmissions have 2ch 32k AC-3 streams
    ret &= Samplerate >= 44100;
    if (!ret)
        return false;
    // Will passthrough if surround audio was defined. Amplifier will
    // do the downmix if required
    ret &= m_maxChannels >= 6 && Channels > 2;
    // Stereo content will always be decoded so it can later be upmixed
    // unless audio is configured for stereo. We can passthrough otherwise
    ret |= m_maxChannels == 2;

    return ret;
}

bool AudioOutput::CanDownmix(void) const
{
    return true;
}

void AudioOutput::SetEffDsp(int DSPRate)
{
    LOG(VB_AUDIO, LOG_INFO,  QString("Setting effectove DSP rate to %1").arg(DSPRate));
    m_effectiveDSPRate = DSPRate;
}

/**
 * Add frames to the audiobuffer and perform any required processing
 *
 * Returns false if there's not enough space right now
 */
bool AudioOutput::AddFrames(void *Buffer, int Frames, qint64 Timecode)
{
    return AddData(Buffer, Frames * m_sourceBytePerFrame, Timecode, Frames);
}

/**
 * Add data to the audiobuffer and perform any required processing
 *
 * Returns false if there's not enough space right now
 */
bool AudioOutput::AddData(void *Buffer, int Length,
                              qint64 Timecode, int Frames)
{
    (void)Frames;
    int frames   = Length / m_sourceBytePerFrame;
    void *buffer = Buffer;
    int bpf      = m_bytesPerFrame;
    int len      = Length;
    bool music   = false;
    int bdiff;

    if (!m_configureSucceeded)
    {
        LOG(VB_GENERAL, LOG_ERR, "AddData called with audio framework not "
                                 "initialised");
        m_lengthLastData = 0;
        return false;
    }

    /* See if we're waiting for new samples to be buffered before we unpause
       post channel change, seek, etc. Wait for 4 fragments to be buffered */
    if (m_unpauseWhenReady && m_pauseAudio && AudioReady() > m_fragmentSize << 2)
    {
        m_unpauseWhenReady = false;
        Pause(false);
    }

    // Don't write new samples if we're resetting the buffer or reconfiguring
    QMutexLocker lock(&m_audioBufferLock);

    uint OriginalWaud = m_waud;
    int  afree    = AudioFree();
    int  used     = kAudioRingBufferSize - afree;

    if (m_passthrough && m_spdifEnc)
    {
        if (m_processing)
        {
            /*
             * We shouldn't encounter this case, but it can occur when
             * timestretch just got activated. So we will just drop the
             * data
             */
            LOG(VB_AUDIO, LOG_INFO,
                "Passthrough activated with audio processing. Dropping audio");
            return false;
        }
        // mux into an IEC958 packet
        m_spdifEnc->WriteFrame((unsigned char *)Buffer, len);
        len = m_spdifEnc->GetProcessedSize();
        if (len > 0)
        {
            buffer = Buffer = m_spdifEnc->GetProcessedBuffer();
            m_spdifEnc->Reset();
            frames = len / m_sourceBytePerFrame;
        }
        else
            frames = 0;
    }
    m_lengthLastData = (qint64)
        ((double)(len * 1000) / (m_sourceSamplerate * m_sourceBytePerFrame));

    LOG(VB_AUDIO, LOG_DEBUG,  QString("AddData frames=%1, bytes=%2, used=%3, free=%4, "
                      "Timecode=%5 needsupmix=%6")
              .arg(frames).arg(len).arg(used).arg(afree).arg(Timecode)
              .arg(m_needsUpmix));

    // Mythmusic doesn't give us timestamps
    if (Timecode < 0)
    {
        Timecode = (m_framesBuffered * 1000) / m_sourceSamplerate;
        m_framesBuffered += frames;
        music = true;
    }

    if (HasListeners())
    {
        // Send original samples to any attached visualisations
        UpdateListeners((unsigned char *)Buffer, len, Timecode, m_sourceChannels,
                       m_outputSettings->FormatToBits(m_format));
    }

    // Calculate amount of free space required in ringbuffer
    if (m_processing)
    {
        // Final float conversion space requirement
        len = sizeof(*m_sourceInputBuffer) / AudioOutputSettings::SampleSize(m_format) * len;

        // Account for changes in number of channels
        if (m_needsDownmix)
            len = (len * m_configuredChannels ) / m_sourceChannels;

        // Check we have enough space to write the data
        if (m_needResampler && m_sourceState)
            len = (int)ceilf(float(len) * m_sourceData.src_ratio);

        if (m_needsUpmix)
            len = (len * m_configuredChannels ) / m_sourceChannels;

        // Include samples in upmix buffer that may be flushed
        if (m_needsUpmix && m_upmixer)
            len += m_upmixer->numUnprocessedFrames() * bpf;

        // Include samples in soundstretch buffers
        if (m_soundStretch)
            len += (m_soundStretch->numUnprocessedSamples() +
                    (int)(m_soundStretch->numSamples() / m_stretchFactor)) * bpf;
    }

    if (len > afree)
    {
        LOG(VB_AUDIO, LOG_DEBUG,  "Buffer is full, AddData returning false");
        return false; // would overflow
    }

    int frames_remaining = frames;
    int frames_final = 0;
    int maxframes = (kAudioSRCInputSize / m_sourceChannels) & ~0xf;
    int offset = 0;

    while (frames_remaining > 0)
    {
        buffer = (char *)Buffer + offset;
        frames = frames_remaining;
        len = frames * m_sourceBytePerFrame;

        if (m_processing)
        {
            if (frames > maxframes)
            {
                frames = maxframes;
                len = frames * m_sourceBytePerFrame;
                offset += len;
            }
            // Convert to floats
            len = AudioOutputUtil::ToFloat(m_format, m_sourceInput, buffer, len);
        }

        frames_remaining -= frames;

        // Perform downmix if necessary
        if (m_needsDownmix)
            if(AudioOutputDownmix::DownmixFrames(m_sourceChannels,
                                                 m_configuredChannels,
                                                 m_sourceInput, m_sourceInput, frames) < 0)
                LOG(VB_GENERAL, LOG_ERR,   "Error occurred while downmixing");

        // Resample if necessary
        if (m_needResampler && m_sourceState)
        {
            m_sourceData.input_frames = frames;
            int error = src_process(m_sourceState, &m_sourceData);

            if (error)
                LOG(VB_GENERAL, LOG_ERR,   QString("Error occurred while resampling audio: %1")
                        .arg(src_strerror(error)));

            buffer = m_sourceOutput;
            frames = m_sourceData.output_frames_gen;
        }
        else if (m_processing)
        {
            buffer = m_sourceInput;
        }

        /* we want the timecode of the last sample added but we are given the
           timecode of the first - add the time in ms that the frames added
           represent */

        // Copy samples into audiobuffer, with upmix if necessary
        if ((len = CopyWithUpmix((char *)buffer, frames, OriginalWaud)) <= 0)
        {
            continue;
        }

        frames = len / bpf;
        frames_final += frames;

        bdiff = kAudioRingBufferSize - m_waud;
        if ((len % bpf) != 0 && bdiff < len)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("AddData: Corruption likely: len = %1 (bpf = %2)")
                    .arg(len)
                    .arg(bpf));
        }

        if ((bdiff % bpf) != 0 && bdiff < len)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("AddData: Corruption likely: bdiff = %1 (bpf = %2)")
                    .arg(bdiff)
                    .arg(bpf));
        }

        if (m_soundStretch)
        {
            // does not change the timecode, only the number of samples
            OriginalWaud     = m_waud;
            int bdFrames = bdiff / bpf;

            if (bdiff < len)
            {
                m_soundStretch->putSamples((STST *)(WPOS), bdFrames);
                m_soundStretch->putSamples((STST *)ABUF, (len - bdiff) / bpf);
            }
            else
                m_soundStretch->putSamples((STST *)(WPOS), frames);

            int nFrames = m_soundStretch->numSamples();
            if (nFrames > frames)
                CheckFreeSpace(nFrames);

            len = nFrames * bpf;

            if (nFrames > bdFrames)
            {
                nFrames -= m_soundStretch->receiveSamples((STST *)(WPOS),
                                                         bdFrames);
                OriginalWaud = 0;
            }
            if (nFrames > 0)
                nFrames = m_soundStretch->receiveSamples((STST *)(WPOS),
                                                        nFrames);

            OriginalWaud = (OriginalWaud + nFrames * bpf) % kAudioRingBufferSize;
        }

        if (m_internalVolumeControl && SWVolume())
        {
            OriginalWaud    = m_waud;
            int num     = len;

            if (bdiff <= num)
            {
                AudioOutputUtil::AdjustVolume(WPOS, bdiff, m_softwareVolume,
                                              music, m_needsUpmix && m_upmixer);
                num -= bdiff;
                OriginalWaud = 0;
            }
            if (num > 0)
                AudioOutputUtil::AdjustVolume(WPOS, num, m_softwareVolume,
                                              music, m_needsUpmix && m_upmixer);
            OriginalWaud = (OriginalWaud + num) % kAudioRingBufferSize;
        }

        if (m_digitalEncoder)
        {
            OriginalWaud            = m_waud;
            int to_get          = 0;

            if (bdiff < len)
            {
                m_digitalEncoder->Encode(WPOS, bdiff, m_processing ? FORMAT_FLT : m_format);
                to_get = m_digitalEncoder->Encode(ABUF, len - bdiff,
                                         m_processing ? FORMAT_FLT : m_format);
            }
            else
            {
                to_get = m_digitalEncoder->Encode(WPOS, len,
                                         m_processing ? FORMAT_FLT : m_format);
            }

            if (bdiff <= to_get)
            {
                m_digitalEncoder->GetFrames(WPOS, bdiff);
                to_get -= bdiff ;
                OriginalWaud = 0;
            }
            if (to_get > 0)
                m_digitalEncoder->GetFrames(WPOS, to_get);

            OriginalWaud = (OriginalWaud + to_get) % kAudioRingBufferSize;
        }

        m_waud = OriginalWaud;
    }

    SetAudiotime(frames_final, Timecode);

    return true;
}

bool AudioOutput::NeedDecodingBeforePassthrough(void) const
{
    return false;
}

qint64 AudioOutput::LengthLastData(void) const
{
    return m_lengthLastData;
}

/**
 * Set the timecode of the samples most recently added to the audiobuffer
 *
 * Used by mythmusic for seeking since it doesn't provide timecodes to
 * AddData()
 */
void AudioOutput::SetTimecode(qint64 Timecode)
{
    m_audioBufferTimecode = m_timecode = Timecode;
    m_framesBuffered = (Timecode * m_sourceSamplerate) / 1000;
}

bool AudioOutput::IsPaused(void) const
{
    return m_actuallyPaused;
}

void AudioOutput::Pause(bool Paused)
{
    if (m_unpauseWhenReady)
        return;

    LOG(VB_AUDIO, LOG_INFO,  QString("Pause %1").arg(Paused));

    if (m_pauseAudio != Paused)
        m_wasPaused = m_pauseAudio;

    m_pauseAudio = Paused;
    m_actuallyPaused = false;
}

void AudioOutput::PauseUntilBuffered()
{
    Reset();
    Pause(true);
    m_unpauseWhenReady = true;
}

/**
 * Block until all available frames have been written to the device
 */
void AudioOutput::Drain()
{
    while (AudioReady() > m_fragmentSize)
        TorcUSleep(1000);
}

/**
 * Calculate the timecode of the samples that are about to become audible
 */
qint64 AudioOutput::GetAudiotime(void)
{
    if (m_audioBufferTimecode == 0 || !m_configureSucceeded)
        return 0;

    int obpf = m_outputBytesPerFrame;
    qint64 oldtimecode;

    /* We want to calculate 'm_timecode', which is the timestamp of the audio
       Which is leaving the sound card at this instant.

       We use these variables:

       'm_effectiveDSPRate' is frames/sec

       'm_audioBufferTimecode' is the timecode of the audio that has just been
       written into the buffer.

       'totalbuffer' is the total # of bytes in our audio buffer, and the
       sound card's buffer. */


    QMutexLocker lockav(&m_avSyncLock);

    int soundcard_buffer = GetBufferedOnSoundcard(); // bytes

    /* AudioReady tells us how many bytes are in audiobuffer
       scaled appropriately if output format != internal format */
    int main_buffer = AudioReady();

    oldtimecode = m_timecode;

    /* timecode is the stretch adjusted version
       of major post-stretched buffer contents
       processing latencies are catered for in AddData/SetAudiotime
       to eliminate race */
    m_timecode = m_audioBufferTimecode - (m_effectiveDSPRate && obpf ? (
        ((qint64)(main_buffer + soundcard_buffer) * m_effectiveStretchFactor) /
        (m_effectiveDSPRate * obpf)) : 0);

    /* m_timecode should never go backwards, but we might get a negative
       value if GetBufferedOnSoundcard() isn't updated by the driver very
       quickly (e.g. ALSA) */
    if (m_timecode < oldtimecode)
        m_timecode = oldtimecode;

    LOG(VB_AUDIO, LOG_DEBUG,  QString("GetAudiotime audt=%1 atc=%2 mb=%3 sb=%4 tb=%5 "
                      "sr=%6 obpf=%7 bpf=%8 sf=%9 %10 %11")
              .arg(m_timecode).arg(m_audioBufferTimecode)
              .arg(main_buffer)
              .arg(soundcard_buffer)
              .arg(main_buffer+soundcard_buffer)
              .arg(m_samplerate).arg(obpf).arg(m_bytesPerFrame).arg(m_stretchFactor)
              .arg((main_buffer + soundcard_buffer) * m_effectiveStretchFactor)
              .arg(((main_buffer + soundcard_buffer) * m_effectiveStretchFactor ) /
                   (m_effectiveDSPRate * obpf))
              );

    return m_timecode;
}

/**
 * Get the difference in timecode between the samples that are about to become
 * audible and the samples most recently added to the audiobuffer, i.e. the
 * time in ms representing the sum total of buffered samples
 */
qint64 AudioOutput::GetAudioBufferedTime(void)
{
    qint64 ret = m_audioBufferTimecode - GetAudiotime();
    // Pulse can give us values that make this -ve
    if (ret < 0)
        return 0;
    return ret;
}

/**
 * Set the bitrate of the source material, reported in periodic OutputEvents
 */
void AudioOutput::SetSourceBitrate(int Rate)
{
    if (Rate > 0)
        m_sourceBitrate = Rate;
}

int AudioOutput::GetFillStatus(void)
{
    return kAudioRingBufferSize - AudioFree();
}

/**
 * Fill in the number of bytes in the audiobuffer and the total
 * size of the audiobuffer
 */
void AudioOutput::GetBufferStatus(uint &Fill, uint &Total)
{
    Fill  = kAudioRingBufferSize - AudioFree();
    Total = kAudioRingBufferSize;
}

void AudioOutput::BufferOutputData(bool Buffer)
{
    m_bufferOutputDataForUse = Buffer;
}

int AudioOutput::ReadOutputData(unsigned char *ReadBuffer, int MaxLength)
{
    (void)ReadBuffer;
    (void)MaxLength;
    return 0;
}

bool AudioOutput::IsUpmixing(void)
{
    return m_needsUpmix && m_upmixer;
}

/**
 * Toggle between stereo and upmixed 5.1 if the source material is stereo
 */
bool AudioOutput::ToggleUpmix(void)
{
    // Can only upmix from mono/stereo to 6 ch
    if (m_maxChannels == 2 || m_sourceChannels > 2 || m_passthrough)
        return false;

    m_upmixDefault = !m_upmixDefault;

    const AudioSettings settings(m_format, m_sourceChannels, m_codec,
                                 m_sourceSamplerate, m_passthrough);
    Reconfigure(settings);
    return IsUpmixing();
}

/**
 * Upmixing of the current source is available if requested
 */
bool AudioOutput::CanUpmix(void)
{
    return !m_passthrough && m_sourceChannels <= 2 && m_maxChannels > 2;
}

bool AudioOutput::IsPulseAudioRunning(void)
{
    QStringList args("-ax");
    QProcess pulse;
    pulse.start("ps", args);

    if (pulse.waitForStarted(2000))
    {
        if (pulse.waitForFinished(2000) && (pulse.exitStatus() == QProcess::NormalExit))
        {
            QByteArray results = pulse.readAll();
            if (results.contains("pulseaudio"))
            {
                LOG(VB_GENERAL, LOG_INFO, "Pulseaudio is running");
                return true;
            }

            return false;
        }
    }

    LOG(VB_GENERAL, LOG_ERR, "Failed to check pulse status");
    return false;
}

bool AudioOutput::IsErrored(void)
{
    return m_configError;
}

bool AudioOutput::SetupPassthrough(int Codec, int CodecProfile, int &TempSamplerate, int &TempChannels)
{
    if (Codec == CODEC_ID_DTS && !m_outputSettingsDigital->CanFeature(FEATURE_DTSHD))
    {
        // We do not support DTS-HD bitstream so force extraction of the
        // DTS core track instead
        CodecProfile = FF_PROFILE_DTS;
    }

    QString log = AudioOutputSettings::GetPassthroughParams(
        Codec, CodecProfile,
        TempSamplerate, TempChannels,
        m_outputSettingsDigital->GetMaxHDRate() == 768000);
    LOG(VB_AUDIO, LOG_INFO,  "Setting " + log + " passthrough");

    if (m_spdifEnc)
        delete m_spdifEnc;

    m_spdifEnc = new AudioSPDIFEncoder("spdif", Codec);
    if (m_spdifEnc->Succeeded() && Codec == CODEC_ID_DTS)
    {
        switch (CodecProfile)
        {
            case FF_PROFILE_DTS:
            case FF_PROFILE_DTS_ES:
            case FF_PROFILE_DTS_96_24:
                m_spdifEnc->SetMaxHDRate(0);
                break;
            case FF_PROFILE_DTS_HD_HRA:
            case FF_PROFILE_DTS_HD_MA:
                m_spdifEnc->SetMaxHDRate(TempSamplerate * TempChannels / 2);
                break;
        }
    }

    if (!m_spdifEnc->Succeeded())
    {
        delete m_spdifEnc;
        m_spdifEnc = NULL;
        return false;
    }
    return true;
}

AudioOutputSettings *AudioOutput::OutputSettings(bool Digital)
{
    if (Digital)
        return m_outputSettingsDigital;
    return m_outputSettings;
}

/**
 * Copy frames into the audiobuffer, upmixing en route if necessary
 *
 * Returns the number of frames written, which may be less than requested
 * if the upmixer buffered some (or all) of them
 */
int AudioOutput::CopyWithUpmix(char *Buffer, int Frames, uint &OriginalWaud)
{
    int len   = CheckFreeSpace(Frames);
    int bdiff = kAudioRingBufferSize - OriginalWaud;
    int bpf   = m_bytesPerFrame;
    int off   = 0;

    if (!m_needsUpmix)
    {
        int num  = len;

        if (bdiff <= num)
        {
            memcpy(WPOS, Buffer, bdiff);
            num -= bdiff;
            off = bdiff;
            OriginalWaud = 0;
        }
        if (num > 0)
            memcpy(WPOS, Buffer + off, num);
        OriginalWaud = (OriginalWaud + num) % kAudioRingBufferSize;
        return len;
    }

    // Convert mono to stereo as most devices can't accept mono
    if (m_configuredChannels == 2 && m_sourceChannels == 1)
    {
        int bdFrames = bdiff / bpf;
        if (bdFrames <= Frames)
        {
            AudioOutputUtil::MonoToStereo(WPOS, Buffer, bdFrames);
            Frames -= bdFrames;
            off = bdFrames * sizeof(float); // 1 channel of floats
            OriginalWaud = 0;
        }
        if (Frames > 0)
            AudioOutputUtil::MonoToStereo(WPOS, Buffer + off, Frames);

        OriginalWaud = (OriginalWaud + Frames * bpf) % kAudioRingBufferSize;
        return len;
    }

    // Upmix to 6ch via FreeSurround
    // Calculate frame size of input
    off =  m_processing ? sizeof(float) : m_outputSettings->SampleSize(m_format);
    off *= m_sourceChannels;

    int i = 0;
    len = 0;
    int nFrames, bdFrames;
    while (i < Frames)
    {
        i += m_upmixer->putFrames(Buffer + i * off, Frames - i, m_sourceChannels);
        nFrames = m_upmixer->numFrames();
        if (!nFrames)
            continue;

        len += CheckFreeSpace(nFrames);

        bdFrames = (kAudioRingBufferSize - OriginalWaud) / bpf;
        if (bdFrames < nFrames)
        {
            if ((OriginalWaud % bpf) != 0)
            {
                LOG(VB_GENERAL, LOG_ERR,   QString("Upmixing: OriginalWaud = %1 (bpf = %2)")
                        .arg(OriginalWaud)
                        .arg(bpf));
            }
            m_upmixer->receiveFrames((float *)(WPOS), bdFrames);
            nFrames -= bdFrames;
            OriginalWaud = 0;
        }
        if (nFrames > 0)
            m_upmixer->receiveFrames((float *)(WPOS), nFrames);

        OriginalWaud = (OriginalWaud + nFrames * bpf) % kAudioRingBufferSize;
    }
    return len;
}

/**
 * Set the timecode of the top of the ringbuffer
 * Exclude all other processing elements as they dont vary
 * between AddData calls
 */
void AudioOutput::SetAudiotime(int Frames, qint64 Timecode)
{
    qint64 processframes_stretched   = 0;
    qint64 processframes_unstretched = 0;
    qint64 oldaudiobuffertimecode    = m_audioBufferTimecode;

    if (!m_configureSucceeded)
        return;

    if (m_needsUpmix && m_upmixer)
        processframes_unstretched -= m_upmixer->frameLatency();

    if (m_soundStretch)
    {
        processframes_unstretched -= m_soundStretch->numUnprocessedSamples();
        processframes_stretched   -= m_soundStretch->numSamples();
    }

    if (m_digitalEncoder)
    {
        processframes_stretched -= m_digitalEncoder->Buffered();
    }

    m_audioBufferTimecode =
        Timecode + (m_effectiveDSPRate ? ((Frames + processframes_unstretched * 100000) +
                            (processframes_stretched * m_effectiveStretchFactor)
                       ) / m_effectiveDSPRate : 0);

    // check for timecode wrap and reset m_timecode if detected
    // timecode will always be monotonic asc if not seeked and reset
    // happens if seek or pause happens
    if (m_audioBufferTimecode < oldaudiobuffertimecode)
        m_timecode = 0;

    LOG(VB_AUDIO, LOG_DEBUG,  QString("SetAudiotime atc=%1 tc=%2 f=%3 pfu=%4 pfs=%5")
              .arg(m_audioBufferTimecode)
              .arg(Timecode)
              .arg(Frames)
              .arg(processframes_unstretched)
              .arg(processframes_stretched));
}

void AudioOutput::KillAudio(void)
{
    m_killAudioLock.lock();

    LOG(VB_AUDIO, LOG_INFO, "Killing AudioOutput");

    StopOutputThread();

    QMutexLocker lock(&m_audioBufferLock);

    if (m_soundStretch)
    {
        delete m_soundStretch;
        m_soundStretch = NULL;
        m_oldStretchFactor = m_stretchFactor;
        m_stretchFactor = 1.0f;
    }

    if (m_digitalEncoder)
    {
        delete m_digitalEncoder;
        m_digitalEncoder = NULL;
    }

    if (m_upmixer)
    {
        delete m_upmixer;
        m_upmixer = NULL;
    }

    if (m_sourceState)
    {
        src_delete(m_sourceState);
        m_sourceState = NULL;
    }

    m_needsUpmix = m_needResampler = m_encode = false;

    CloseDevice();

    m_killAudioLock.unlock();
}

/**
 * Set the timestretch factor
 *
 * You must hold the audioBufferLock to call this safely
 */
void AudioOutput::SetStretchFactorLocked(float Factor)
{
    if (m_stretchFactor == Factor && m_soundStretch)
        return;

    m_stretchFactor = Factor;

    int channels = m_needsUpmix || m_needsDownmix ?
        m_configuredChannels : m_sourceChannels;
    if (channels < 1 || channels > 8 || !m_configureSucceeded)
        return;

    bool willstretch = m_stretchFactor < 0.99f || m_stretchFactor > 1.01f;
    m_effectiveStretchFactor = (int)(100000.0f * Factor + 0.5);

    if (m_soundStretch)
    {
        if (!willstretch && m_forcedProcessing)
        {
            m_forcedProcessing = false;
            m_processing = false;
            delete m_soundStretch;
            m_soundStretch = NULL;
            LOG(VB_GENERAL, LOG_INFO, QString("Cancelling time stretch"));
            m_bytesPerFrame = m_previousBytesPerFrame;
            m_waud = m_raud = 0;
            m_resetActive->Ref();
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Changing time stretch to %1").arg(m_stretchFactor));
            m_soundStretch->setTempo(m_stretchFactor);
        }
    }
    else if (willstretch)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Using time stretch %1").arg(m_stretchFactor));
        m_soundStretch = new soundtouch::SoundTouch();
        m_soundStretch->setSampleRate(m_samplerate);
        m_soundStretch->setChannels(channels);
        m_soundStretch->setTempo(m_stretchFactor);
        m_soundStretch->setSetting(SETTING_SEQUENCE_MS, 35);
        /* If we weren't already processing we need to turn on float conversion
           adjust sample and frame sizes accordingly and dump the contents of
           the audiobuffer */
        if (!m_processing)
        {
            m_processing = true;
            m_forcedProcessing = true;
            m_previousBytesPerFrame = m_bytesPerFrame;
            m_bytesPerFrame = m_sourceChannels *
                              AudioOutputSettings::SampleSize(FORMAT_FLT);
            m_waud = m_raud = 0;
            m_resetActive->Ref();
        }
    }
}

/**
 * Get the number of bytes in the audiobuffer
 */
inline int AudioOutput::AudioLength()
{
    if (m_waud >= m_raud)
        return m_waud - m_raud;
    else
        return kAudioRingBufferSize - (m_raud - m_waud);
}

/**
 * Get the free space in the audiobuffer in bytes
 */
int AudioOutput::AudioFree()
{
    return kAudioRingBufferSize - AudioLength() - 1;
    /* There is one wasted byte in the buffer. The case where m_waud = m_raud is
       interpreted as an empty buffer, so the fullest the buffer can ever
       be is kAudioRingBufferSize - 1. */
}

/**
 * Get the scaled number of bytes in the audiobuffer, i.e. the number of
 * samples * the output bytes per sample
 *
 * This value can differ from that returned by AudioLength if samples are
 * being converted to floats and the output sample format is not 32 bits
 */
int AudioOutput::AudioReady()
{
    if (m_passthrough || m_encode || m_bytesPerFrame == m_outputBytesPerFrame)
        return AudioLength();
    else
        return AudioLength() * m_outputBytesPerFrame / m_bytesPerFrame;
}

/**
 * Check that there's enough space in the audiobuffer to write the provided
 * number of frames
 *
 * If there is not enough space, set 'Frames' to the number that will fit
 *
 * Returns the number of bytes that the frames will take up
 */
int AudioOutput::CheckFreeSpace(int &Frames)
{
    int bpf   = m_bytesPerFrame;
    int len   = Frames * bpf;
    int afree = AudioFree();

    if (len <= afree)
        return len;

    LOG(VB_GENERAL, LOG_ERR,   QString("Audio buffer overflow, %1 frames lost!")
            .arg(Frames - (afree / bpf)));

    Frames = afree / bpf;
    len = Frames * bpf;

    if (!m_sourceState)
        return len;

    int error = src_reset(m_sourceState);
    if (error)
    {
        LOG(VB_GENERAL, LOG_ERR,   QString("Error occurred while resetting resampler: %1")
                .arg(src_strerror(error)));
        m_sourceState = NULL;
    }

    return len;
}

/**
 * Copy frames from the audiobuffer into the buffer provided
 *
 * If 'FullBuffer' is true we copy either 'size' bytes (if available) or
 * nothing. Otherwise, we'll copy less than 'size' bytes if that's all that's
 * available. Returns the number of bytes copied.
 */
int AudioOutput::GetAudioData(unsigned char *Buffer, int size, bool FullBuffer, volatile uint *LocalRaud)
{

#define LRPOS (m_audioBuffer + *LocalRaud)
    // re-check AudioReady() in case things changed.
    // for example, ClearAfterSeek() might have run
    int avail_size   = AudioReady();
    int frag_size    = size;
    int written_size = size;

    if (LocalRaud == NULL)
        LocalRaud = &m_raud;

    if (!FullBuffer && (size > avail_size))
    {
        // when FullBuffer is false, return any available data
        frag_size = avail_size;
        written_size = frag_size;
    }

    if (!avail_size || (frag_size > avail_size))
        return 0;

    int bdiff = kAudioRingBufferSize - m_raud;

    int obytes = m_outputSettings->SampleSize(m_outputFormat);
    bool FromFloats = m_processing && !m_encode && m_outputFormat != FORMAT_FLT;

    // Scale if necessary
    if (FromFloats && obytes != sizeof(float))
        frag_size *= sizeof(float) / obytes;

    int off = 0;

    if (bdiff <= frag_size)
    {
        if (FromFloats)
            off = AudioOutputUtil::FromFloat(m_outputFormat, Buffer,
                                             LRPOS, bdiff);
        else
        {
            memcpy(Buffer, LRPOS, bdiff);
            off = bdiff;
        }

        frag_size -= bdiff;
        *LocalRaud = 0;
    }
    if (frag_size > 0)
    {
        if (FromFloats)
            AudioOutputUtil::FromFloat(m_outputFormat, Buffer + off,
                                       LRPOS, frag_size);
        else
            memcpy(Buffer + off, LRPOS, frag_size);
    }

    *LocalRaud += frag_size;

    // Mute individual channels through mono->stereo duplication
    MuteState mute_state = GetMuteState();
    if (!m_encode && !m_passthrough &&
        written_size && m_configuredChannels > 1 &&
        (mute_state == kMuteLeft || mute_state == kMuteRight))
    {
        AudioOutputUtil::MuteChannel(obytes << 3, m_configuredChannels,
                                     mute_state == kMuteLeft ? 0 : 1,
                                     Buffer, written_size);
    }

    return written_size;
}

void AudioOutput::run(void)
{
    RunProlog();
    LOG(VB_GENERAL, LOG_INFO, QString("Starting audio thread"));

    {
        unsigned char* zeros          = new unsigned char[m_fragmentSize];
        unsigned char* fragmentbuffer = new unsigned char[m_fragmentSize + 16];
        unsigned char* fragment       = (unsigned char *)AOALIGN(fragmentbuffer[0]);
        memset(zeros, 0, m_fragmentSize);

        // to reduce startup latency, write silence in 8ms chunks
        int zerofragmentsize = 8 * m_samplerate * m_outputBytesPerFrame / 1000;
        if (zerofragmentsize > m_fragmentSize)
            zerofragmentsize = m_fragmentSize;

        while (!m_killAudio)
        {
            if (m_pauseAudio)
            {
                if (!m_actuallyPaused)
                {
                    LOG(VB_AUDIO, LOG_INFO,  "OutputAudioLoop: audio paused");
                    m_wasPaused = true;
                }

                m_actuallyPaused = true;
                m_timecode = 0; // mark 'm_timecode' as invalid.

                WriteAudio(zeros, zerofragmentsize);

                if (m_parent)
                    m_parent->SetAudioTime(GetAudiotime(), GetMicrosecondCount());

                continue;
            }
            else
            {
                if (m_wasPaused)
                    m_wasPaused = false;
            }

            /* do audio output */
            int ready = AudioReady();

            // wait for the buffer to fill with enough to play
            if (m_fragmentSize > ready)
            {
                if (ready > 0)  // only log if we're sending some audio
                    LOG(VB_AUDIO, LOG_DEBUG,  QString("audio waiting for buffer to fill: "
                                      "have %1 want %2")
                              .arg(ready).arg(m_fragmentSize));

                TorcUSleep(10000);
                continue;
            }

            // delay setting m_raud until after phys buffer is filled
            // so GetAudiotime will be accurate without locking
            m_resetActive->TestAndDeref();
            volatile uint nextraud = m_raud;
            if (GetAudioData(fragment, m_fragmentSize, true, &nextraud))
            {
                if (!m_resetActive->TestAndDeref())
                {
                    WriteAudio(fragment, m_fragmentSize);

                    if (m_parent)
                        m_parent->SetAudioTime(GetAudiotime(), GetMicrosecondCount());

                    if (!m_resetActive->TestAndDeref())
                        m_raud = nextraud;
                }
            }
        }

        delete [] zeros;
        delete [] fragmentbuffer;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Stopping audio thread"));
    RunEpilog();
}

/**
 * Reset the audiobuffer, timecode and visualisations
 */
void AudioOutput::Reset()
{
    QMutexLocker lock(&m_audioBufferLock);
    QMutexLocker lockav(&m_avSyncLock);

    m_audioBufferTimecode = m_timecode = m_framesBuffered = 0;
    if (m_digitalEncoder)
    {
        m_waud = m_raud = 0;    // empty ring buffer
        memset(m_audioBuffer, 0, kAudioRingBufferSize);
    }
    else
    {
        m_waud = m_raud;        // empty ring buffer
    }
    m_resetActive->Ref();
    m_currentSeconds = -1;
    m_wasPaused = !m_pauseAudio;
    // clear any state that could remember previous audio in any active filters
    if (m_needsUpmix && m_upmixer)
        m_upmixer->flush();
    if (m_soundStretch)
        m_soundStretch->clear();
    if (m_digitalEncoder)
        m_digitalEncoder->Clear();

    // Setup visualisations, zero the visualisations buffers
    PrepareListeners();
}

int AudioOutput::GetBaseAudBufTimeCode(void) const
{
    return m_audioBufferTimecode;
}

bool AudioOutput::StartOutputThread(void)
{
    if (m_haveAudioThread)
        return true;

    start();

    m_haveAudioThread = true;
    return true;
}


void AudioOutput::StopOutputThread(void)
{
    m_killAudio = true;

    if (m_haveAudioThread)
    {
        wait();
        m_haveAudioThread = false;
    }
}

AudioDeviceConfig* AudioOutput::GetAudioDeviceConfig(const QString &Name, const QString &Description)
{
    AudioOutput *audio = OpenAudio(Name, NULL);
    AudioOutputSettings settings = *(audio->GetOutputSettingsCleaned());
    delete audio;

    if (settings.IsInvalid())
    {
        QString msg = QObject::tr("Invalid or unuseable audio device");
         return new AudioDeviceConfig(Name, msg);
    }

    QString capabilities = Description;

    if (settings.HasELD())
        capabilities += QObject::tr(" (%1 connected to %2)").arg(settings.GetELD().GetProductName()).arg(settings.GetELD().GetConnectionName());

    QString speakers;
    int maxchannels = settings.BestSupportedChannelsELD();
    switch (maxchannels)
    {
        case 6:
            speakers = "5.1";
            break;
        case 8:
            speakers = "7.1";
            break;
        default:
            speakers = "2.0";
            break;
    }

    capabilities += QObject::tr(" Supports up to %1").arg(speakers);

    if (settings.CanPassthrough() != PassthroughNo)
    {
        if (settings.HasELD())
        {
            capabilities += " (" + settings.GetELD().GetCodecsDescription() + ")";
        }
        else
        {
            int mask = (settings.CanLPCM() << 0) | (settings.CanAC3()  << 1) |(settings.CanDTS()  << 2);
            static const QString names[3] = { QObject::tr("LPCM"), QObject::tr("AC3"), QObject::tr("DTS") };

            if (mask != 0)
            {
                capabilities += QObject::tr(" (guessing: ");
                bool found_one = false;
                for (unsigned int i = 0; i < 3; i++)
                {
                    if ((mask & (1 << i)) != 0)
                    {
                        if (found_one)
                            capabilities += ", ";
                        capabilities += names[i];
                        found_one = true;
                    }
                }
                capabilities += QString(")");
            }
        }
    }

    LOG(VB_AUDIO, LOG_INFO, QString("Found %1 (%2)").arg(Name).arg(capabilities));
    AudioDeviceConfig* config = new AudioDeviceConfig(Name, capabilities);
    config->m_settings = settings;
    return config;
}

AudioFactory* AudioFactory::gAudioFactory = NULL;

AudioFactory::AudioFactory()
{
    nextAudioFactory = gAudioFactory;
    gAudioFactory = this;
}

AudioFactory::~AudioFactory()
{
}

AudioFactory* AudioFactory::GetAudioFactory(void)
{
    return gAudioFactory;
}

AudioFactory* AudioFactory::NextFactory(void) const
{
    return nextAudioFactory;
}
