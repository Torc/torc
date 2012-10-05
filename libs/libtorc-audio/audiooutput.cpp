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

#ifdef Q_OS_WIN
#include "audiooutputdx.h"
#include "audiooutputwin.h"
#endif
#if CONFIG_OSS_OUTDEV
#include "audiooutputoss.h"
#endif
#if CONFIG_ALSA_OUTDEV
#include "audiooutputalsa.h"
#endif
#ifdef Q_OS_MAC
#include "audiooutputosx.h"
#endif
#if CONFIG_LIBPULSE
#include "audiopulsehandler.h"
#endif

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

void AudioOutput::Cleanup(void)
{
#if CONFIG_LIBPULSE
    PulseHandler::Suspend(PulseHandler::kPulseCleanup);
#endif
}

AudioOutput *AudioOutput::OpenAudio(const QString &MainDevice,
                                    const QString &PassthroughDevice,
                                    AudioFormat Format,
                                    int Channels,
                                    int Codec,
                                    int Samplerate,
                                    AudioOutputSource Source,
                                    bool SetInitialVolume,
                                    bool Passthrough,
                                    int UpmixerStartup,
                                    AudioOutputSettings *Custom)
{
    AudioSettings settings(MainDevice, PassthroughDevice, Format, Channels, Codec, Samplerate,
                           Source, SetInitialVolume, Passthrough, UpmixerStartup, Custom);
    return OpenAudio(settings);
}

AudioOutput *AudioOutput::OpenAudio(const QString &MainDevice,
                                    const QString &PassthroughDevice,
                                    bool WillSuspendPulse)
{
    AudioSettings settings(MainDevice, PassthroughDevice);
    return OpenAudio(settings, WillSuspendPulse);
}

AudioOutput *AudioOutput::OpenAudio(AudioSettings &Settings,
                                    bool WillSuspendPulse)
{
    QString &device = Settings.m_mainDevice;
    AudioOutput *ret = NULL;

#if CONFIG_LIBPULSE
    bool pulsestatus = false;
#else
    {
        static bool warned = false;
        if (!warned && IsPulseAudioRunning())
        {
            warned = true;
            LOG(VB_GENERAL, LOG_WARNING,
                "WARNING: ***Pulse Audio is running***");
        }
    }
#endif

    Settings.FixPassThrough();

    if (device.startsWith("PulseAudio:"))
    {
#if CONFIG_LIBPULSE_DISABLED_DELIBERATELY
        return new AudioOutputPulseAudio(Settings);
#else
        LOG(VB_GENERAL, LOG_ERR, "Audio output device is set to PulseAudio "
                                 "but PulseAudio support is not compiled in!");
        return NULL;
#endif
    }
    else if (device.startsWith("NULL"))
    {
        return new AudioOutputNULL(Settings);
    }

#if CONFIG_LIBPULSE
    if (WillSuspendPulse)
    {
        bool ispulse = false;
#if CONFIG_ALSA_OUTDEV
        // Check if using ALSA, that the device doesn't contain the word
        // "pulse" in its hint
        if (device.startsWith("ALSA:"))
        {
            QString device_name = device;

            device_name.remove(0, 5);
            QMap<QString, QString> alsadevs = AudioOutputALSA::GetDevices("pcm");
            if (!alsadevs.empty() && alsadevs.contains(device_name))
            {
                if (alsadevs.value(device_name).contains("pulse",
                                                          Qt::CaseInsensitive))
                {
                    ispulse = true;
                }
            }
        }
#endif
        if (device.contains("pulse", Qt::CaseInsensitive))
        {
            ispulse = true;
        }

        if (!ispulse)
        {
            pulsestatus = PulseHandler::Suspend(PulseHandler::kPulseSuspend);
        }
    }
#endif

    if (device.startsWith("ALSA:"))
    {
#if CONFIG_ALSA_OUTDEV
        Settings.TrimDeviceType();
        ret = new AudioOutputALSA(Settings);
#else
        LOG(VB_GENERAL, LOG_ERR, "Audio output device is set to an ALSA device "
                                 "but ALSA support is not compiled in!");
#endif
    }
    else if (device.startsWith("DirectX:"))
    {
#ifdef USING_MINGW
        ret = new AudioOutputDX(Settings);
#else
        LOG(VB_GENERAL, LOG_ERR, "Audio output device is set to DirectX device "
                                 "but DirectX support is not compiled in!");
#endif
    }
    else if (device.startsWith("Windows:"))
    {
#ifdef USING_MINGW
        ret = new AudioOutputWin(Settings);
#else
        LOG(VB_GENERAL, LOG_ERR, "Audio output device is set to a Windows "
                                 "device but Windows support is not compiled "
                                 "in!");
#endif
    }
#if CONFIG_OSS_OUTDEV
    else
        ret = new AudioOutputOSS(Settings);
#elif defined(Q_OS_MAC)
    else
        ret = new AudioOutputOSX(Settings);
#endif

    if (!ret)
    {
        LOG(VB_GENERAL, LOG_CRIT, "No useable audio output driver found.");
        LOG(VB_GENERAL, LOG_ERR, "Don't disable OSS support unless you're "
                                 "not running on Linux.");
#if CONFIG_LIBPULSE
        if (pulsestatus)
            PulseHandler::Suspend(PulseHandler::kPulseResume);
#endif
        return NULL;
    }
#if CONFIG_LIBPULSE
    ret->m_pulseWasSuspended = pulsestatus;
#endif
    return ret;
}

AudioOutput::AudioOutput()
  : AudioVolume(),
    AudioOutputListeners(),
    m_pulseWasSuspended(false)
{
}

AudioOutput::~AudioOutput()
{
#if CONFIG_LIBPULSE
    if (m_pulseWasSuspended)
        PulseHandler::Suspend(PulseHandler::kPulseResume);
#endif
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

QString AudioOutput::GetError(void) const
{
    return m_lastError;
}

QString AudioOutput::GetWarning(void) const
{
    return m_lastWarning;
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

bool AudioOutput::PulseStatus(void)
{
    return m_pulseWasSuspended;
}

void AudioOutput::SilentError(const QString &Message)
{
    m_lastError = Message;
    m_lastError.detach();
}

void AudioOutput::Warn(const QString &Message)
{
    m_lastWarning = Message;
    m_lastWarning.detach();
    LOG(VB_GENERAL, LOG_WARNING, "AudioOutput Warning: " + m_lastWarning);
}

void AudioOutput::ClearError(void)
{
    m_lastError = QString();
}

void AudioOutput::ClearWarning(void)
{
    m_lastWarning = QString();
}

AudioDeviceConfig* AudioOutput::GetAudioDeviceConfig(QString &Name, QString &Description, bool WillSuspendPulse)
{
    AudioOutputSettings aosettings;
    AudioDeviceConfig *adc;

    AudioOutput *ao = OpenAudio(Name, QString(), WillSuspendPulse);
    aosettings = *(ao->GetOutputSettingsCleaned());
    delete ao;

    if (aosettings.IsInvalid())
    {
        if (!WillSuspendPulse)
        {
            return NULL;
        }
        else
        {
            QString msg = QObject::tr("Invalid or unuseable audio device");
            return new AudioDeviceConfig(Name, msg);
        }
    }

    QString capabilities = Description;

    if (aosettings.HasELD())
    {
        capabilities += QObject::tr(" (%1 connected to %2)")
            .arg(aosettings.GetELD().GetProductName())
            .arg(aosettings.GetELD().GetConnectionName());
    }

    QString speakers;
    int maxchannels = aosettings.BestSupportedChannelsELD();
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

    if (aosettings.CanPassthrough() >= 0)
    {
        if (aosettings.HasELD())
        {
            capabilities += " (" + aosettings.GetELD().GetCodecsDescription() + ")";
        }
        else 
        {
            int mask = (aosettings.CanLPCM() << 0) |
                       (aosettings.CanAC3()  << 1) |
                       (aosettings.CanDTS()  << 2);

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
    LOG(VB_AUDIO, LOG_INFO, QString("Found %1 (%2)")
                                .arg(Name).arg(capabilities));
    adc = new AudioDeviceConfig(Name, capabilities);
    adc->m_settings = aosettings;
    return adc;
}

#if CONFIG_OSS_OUTDEV
static void FillSelectionFromDir(const QDir &dir, QList<AudioDeviceConfig> *List)
{
    QFileInfoList il = dir.entryInfoList();
    for (QFileInfoList::Iterator it = il.begin(); it != il.end(); ++it )
    {
        QString name = (*it).absoluteFilePath();
        QString desc = QObject::tr("OSS device");
        AudioDeviceConfig *config = AudioOutput::GetAudioDeviceConfig(name, desc);
        if (!config)
            continue;

        List->append(*config);
        delete config;
    }
}
#endif

QList<AudioDeviceConfig> AudioOutput::GetOutputList(void)
{
    QList<AudioDeviceConfig> list;
    AudioDeviceConfig *adc = NULL;

#if CONFIG_PULSE
    bool pasuspended = PulseHandler::Suspend(PulseHandler::kPulseSuspend);
#endif

#if CONFIG_ALSA_OUTDEV
    QMap<QString, QString> alsadevs = AudioOutputALSA::GetDevices("pcm");

    if (!alsadevs.empty())
    {
        for (QMap<QString, QString>::const_iterator i = alsadevs.begin(); i != alsadevs.end(); ++i)
        {
            QString key = i.key();
            QString desc = i.value();
            QString devname = QString("ALSA:%1").arg(key);

            adc = GetAudioDeviceConfig(devname, desc);
            if (!adc)
                continue;
            list.append(*adc);
            delete adc;
        }
    }
#endif

#if CONFIG_OSS_OUTDEV
    {
        QDir dev("/dev", "dsp*", QDir::Name, QDir::System);
        FillSelectionFromDir(dev, &list);
        dev.setNameFilters(QStringList("adsp*"));
        FillSelectionFromDir(dev, &list);

        dev.setPath("/dev/sound");
        if (dev.exists())
        {
            dev.setNameFilters(QStringList("dsp*"));
            FillSelectionFromDir(dev, &list);
            dev.setNameFilters(QStringList("adsp*"));
            FillSelectionFromDir(dev, &list);
        }
    }
#endif

#ifdef Q_OS_MAC
    {
        QMap<QString, QString> devs = AudioOutputOSX::GetDevices(NULL);
        if (!devs.isEmpty())
        {
            for (QMap<QString, QString>::const_iterator i = devs.begin(); i != devs.end(); ++i)
            {
                QString key = i.key();
                QString desc = i.value();
                QString devname = QString("CoreAudio:%1").arg(key);

                adc = GetAudioDeviceConfig(devname, desc);
                if (!adc)
                    continue;
                list.append(*adc);
                delete adc;
            }
        }

        QString name = "CoreAudio:Default Output Device";
        QString desc = QObject::tr("CoreAudio default output");
        adc = GetAudioDeviceConfig(name, desc);
        if (adc)
        {
            list.append(*adc);
            delete adc;
        }
    }
#endif
#ifdef USING_MINGW
    {
        QString name = "Windows:";
        QString desc = "Windows default output";
        adc = GetAudioDeviceConfig(name, desc);
        if (adc)
        {
            list.append(*adc);
            delete adc;
        }

        QMap<int, QString> *dxdevs = AudioOutputDX::GetDXDevices();

        if (!dxdevs->empty())
        {
            for (QMap<int, QString>::const_iterator i = dxdevs->begin();
                 i != dxdevs->end(); ++i)
            {
                QString desc = i.value();
                QString devname = QString("DirectX:%1").arg(desc);

                adc = GetAudioDeviceConfig(devname, desc);
                if (!adc)
                    continue;
                list.append(*adc);
                delete adc;
            }
        }
        delete dxdevs;
    }
#endif

#if CONFIG_PULSE
    if (pasuspended)
        PulseHandler::Suspend(PulseHandler::kPulseResume);

    {
        QString name = "PulseAudio:default";
        QString desc =  QObject::tr("PulseAudio default sound server.");
        adc = GetAudioDeviceConfig(name, desc);
        if (adc)
        {
            list.append(*adc);
            delete adc;
        }
    }
#endif

    QString name = "NULL";
    QString desc = "NULL device";
    adc = GetAudioDeviceConfig(name, desc);
    if (adc)
    {
        list.append(*adc);
        delete adc;
    }

    return list;
}
