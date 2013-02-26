/* class AudioOutput
*
* This file is part of the Torc project.
*
* Adapted from the AudioOutput class which is part of the MythTV project.
* Copyright various authors.
*/

// Qt
#include <QDateTime>
#include <QProcess>
#include <QFile>
#include <QDir>

// Torc
#include "torcconfig.h"
#include "torccompat.h"
#include "audiooutputnull.h"
#include "audiooutput.h"

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

AudioOutput* AudioOutput::OpenAudio(const QString &Name)
{
    AudioSettings settings(Name);
    return OpenAudio(settings);
}

AudioOutput *AudioOutput::OpenAudio(const AudioSettings &Settings)
{
    AudioOutput *output = NULL;

    int score = 0;
    AudioFactory* factory = AudioFactory::GetAudioFactory();
    for ( ; factory; factory = factory->NextFactory())
        (void)factory->Score(Settings, score);

    factory = AudioFactory::GetAudioFactory();
    for ( ; factory; factory = factory->NextFactory())
    {
        output = factory->Create(Settings, score);
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

AudioOutput::AudioOutput()
  : AudioVolume(),
    AudioOutputListeners(),
    m_configError(false)
{
}

AudioOutput::~AudioOutput()
{
}

void AudioOutput::SetStretchFactor(float Factor)
{
    (void) Factor;
}

float AudioOutput::GetStretchFactor(void) const
{
    return 1.0f;
}

int AudioOutput::GetChannels(void) const
{
    return 2;
}

AudioFormat AudioOutput::GetFormat(void) const
{
    return FORMAT_S16;
}

int AudioOutput::GetBytesPerFrame(void) const
{
    return 4;
}

AudioOutputSettings* AudioOutput::GetOutputSettingsCleaned(bool Digital)
{
    (void) Digital;
    return new AudioOutputSettings;
}

AudioOutputSettings* AudioOutput::GetOutputSettingsUsers(bool Digital)
{
    (void) Digital;
    return new AudioOutputSettings;
}

bool AudioOutput::CanPassthrough(int Samplerate, int Channels, int Codec, int Profile) const
{
    (void) Samplerate;
    (void) Channels;
    (void) Codec;
    (void) Profile;
    return false;
}

bool AudioOutput::CanDownmix(void) const
{
    return false;
}

bool AudioOutput::NeedDecodingBeforePassthrough(void) const
{
    return true;
}

qint64 AudioOutput::LengthLastData(void) const
{
    return -1;
}

qint64 AudioOutput::GetAudioBufferedTime(void)
{
    return 0;
}

void AudioOutput::SetSourceBitrate(int Rate)
{
    (void)Rate;
}

int AudioOutput::GetFillStatus(void)
{
    return -1;
}

void AudioOutput::GetBufferStatus(uint &Fill, uint &Total)
{
    Fill = Total = 0;
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

AudioDeviceConfig* AudioOutput::GetAudioDeviceConfig(const QString &Name, const QString &Description)
{
    AudioOutput *audio = OpenAudio(Name);
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
