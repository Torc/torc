// Torc
#include "audiosettings.h"

// startup_upmixer 
AudioSettings::AudioSettings()
  : m_mainDevice(QString()),
    m_passthroughDevice(QString()),
    m_format(FORMAT_NONE),
    m_channels(-1),
    m_codec(0),
    m_codecProfile(-1),
    m_samplerate(-1),
    m_setInitialVolume(false),
    m_usePassthrough(false),
    m_sourceType(AUDIOOUTPUT_UNKNOWN),
    m_upmixer(0),
    m_openOnInit(false),
    m_custom(NULL)
{
}

AudioSettings::AudioSettings(const AudioSettings &Settings)
  : m_mainDevice(Settings.m_mainDevice),
    m_passthroughDevice(Settings.m_passthroughDevice),
    m_format(Settings.m_format),
    m_channels(Settings.m_channels),
    m_codec(Settings.m_codec),
    m_codecProfile(Settings.m_codecProfile),
    m_samplerate(Settings.m_samplerate),
    m_setInitialVolume(Settings.m_setInitialVolume),
    m_usePassthrough(Settings.m_usePassthrough),
    m_sourceType(Settings.m_sourceType),
    m_upmixer(Settings.m_upmixer),
    m_openOnInit(true)
{
    if (Settings.m_custom)
    {
        m_custom = new AudioOutputSettings;
        *m_custom = *Settings.m_custom;
    }
    else
    {
        m_custom = NULL;
    }
}

AudioSettings::AudioSettings(const QString &MainDevice,
                             const QString &PassthroughDevice,
                             AudioFormat Format,
                             int Channels,
                             int Codec,
                             int Samplerate,
                             AudioOutputSource Source,
                             bool SetInitialVolume,
                             bool UsePassthrough,
                             int UpmixerStartup,
                             AudioOutputSettings *Custom)
  : m_mainDevice(MainDevice),
    m_passthroughDevice(PassthroughDevice),
    m_format(Format),
    m_channels(Channels),
    m_codec(Codec),
    m_codecProfile(-1),
    m_samplerate(Samplerate),
    m_setInitialVolume(SetInitialVolume),
    m_usePassthrough(UsePassthrough),
    m_sourceType(Source),
    m_upmixer(UpmixerStartup),
    m_openOnInit(true)
{
    if (Custom)
    {
        this->m_custom = new AudioOutputSettings;
        *this->m_custom = *Custom;
    }
    else
    {
        this->m_custom = NULL;
    }
}

AudioSettings::AudioSettings(AudioFormat Format,
                             int Channels,
                             int Codec,
                             int Samplerate,
                             bool UsePassthrough,
                             int UpmixerStartup,
                             int CodecProfile)
  : m_mainDevice(QString()),
    m_passthroughDevice(QString()),
    m_format(Format),
    m_channels(Channels),
    m_codec(Codec),
    m_codecProfile(CodecProfile),
    m_samplerate(Samplerate),
    m_setInitialVolume(false),
    m_usePassthrough(UsePassthrough),
    m_sourceType(AUDIOOUTPUT_UNKNOWN),
    m_upmixer(UpmixerStartup),
    m_openOnInit(true),
    m_custom(NULL)
{
}

AudioSettings::AudioSettings(const QString &MainDevice, const QString &PassthroughDevice)
  : m_mainDevice(MainDevice),
    m_passthroughDevice(PassthroughDevice),
    m_format(FORMAT_NONE),
    m_channels(-1),
    m_codec(0),
    m_codecProfile(-1),
    m_samplerate(-1),
    m_setInitialVolume(false),
    m_usePassthrough(false),
    m_sourceType(AUDIOOUTPUT_UNKNOWN),
    m_upmixer(0),
    m_openOnInit(false),
    m_custom(NULL)
{
}

AudioSettings::~AudioSettings()
{
    if (m_custom)
        delete m_custom;
}

void AudioSettings::FixPassThrough(void)
{
    if (m_passthroughDevice.isEmpty())
        m_passthroughDevice = "auto";
}

void AudioSettings::TrimDeviceType(void)
{
    m_mainDevice.remove(0, 5);
    if (m_passthroughDevice != "auto" && m_passthroughDevice.toLower() != "default")
        m_passthroughDevice.remove(0, 5);
}

QString AudioSettings::GetMainDevice(void) const
{
    QString ret = m_mainDevice;
    ret.detach();
    return ret;
}

QString AudioSettings::GetPassthroughDevice(void) const
{
    QString ret = m_passthroughDevice;
    ret.detach();
    return ret;
}
