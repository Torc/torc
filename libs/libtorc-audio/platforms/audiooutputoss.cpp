// Torc
#include "torclocalcontext.h"
#include "torctimer.h"
#include "audiooutputoss.h"

#if HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#elif HAVE_SOUNDCARD_H
#include <soundcard.h>
#endif

#include <sys/ioctl.h>
#include <fcntl.h>

AudioOutputOSS::AudioOutputOSS(const AudioSettings &Settings)
  : AudioOutputBase(Settings),
    m_audioFd(-1),
    m_mixerFd(-1),
    m_control(SOUND_MIXER_VOLUME)
{
    InitSettings(Settings);
    if (Settings.m_openOnInit)
        Reconfigure(Settings);
}

AudioOutputOSS::~AudioOutputOSS()
{
    KillAudio();
}

AudioOutputSettings* AudioOutputOSS::GetOutputSettings(bool /*digital*/)
{
    AudioOutputSettings *settings = new AudioOutputSettings();

    QByteArray device = m_mainDevice.toAscii();
    m_audioFd = open(device.constData(), O_WRONLY | O_NONBLOCK);

    if (m_audioFd < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error opening audio device (%1)").arg(m_mainDevice));
        delete settings;
        return NULL;
    }

    QList<int> rates = settings->GetRates();
    foreach (int rate, rates)
    {
        int rate2 = rate;
        if (ioctl(m_audioFd, SNDCTL_DSP_SPEED, &rate2) >= 0 && rate2 == rate)
            settings->AddSupportedRate(rate);
    }

    int formats = 0;
    if (ioctl(m_audioFd, SNDCTL_DSP_GETFMTS, &formats) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, "Error retrieving formats");
    }
    else
    {
        QList<AudioFormat> formatlist = settings->GetFormats();
        foreach (AudioFormat format, formatlist)
        {
            int outformat = 0;
            switch (format)
            {
                case FORMAT_U8:  outformat = AFMT_U8;       break;
                case FORMAT_S16: outformat = AFMT_S16_NE;   break;
                default: continue;
            }

            if (formats & outformat)
                settings->AddSupportedFormat(format);
        }
    }

#if defined(AFMT_AC3)
        // Check if drivers supports AC3
    settings->SetPassthrough(formats & AFMT_AC3 ? PassthroughUnknown : PassthroughNo);
#endif

    for (int i = 1; i <= 2; i++)
    {
        int channel = i;

        if (ioctl(m_audioFd, SNDCTL_DSP_CHANNELS, &channel) >= 0 &&
            channel == i)
        {
            settings->AddSupportedChannels(i);
        }
    }

    close(m_audioFd);
    m_audioFd = -1;

    return settings;
}

bool AudioOutputOSS::OpenDevice()
{
    TorcTimer timer;
    timer.Start();

    LOG(VB_AUDIO, LOG_INFO, QString("Opening OSS audio device '%1'.").arg(m_mainDevice));

    while (timer.Elapsed() < 2000 && m_audioFd == -1)
    {
        QByteArray device = m_mainDevice.toAscii();
        m_audioFd = open(device.constData(), O_WRONLY);
        if (m_audioFd < 0 && errno != EAGAIN && errno != EINTR)
        {
            if (errno == EBUSY)
            {
                LOG(VB_GENERAL, LOG_WARNING, QString("Something is currently using: %1.")
                      .arg(m_mainDevice));
                return false;
            }
            LOG(VB_GENERAL, LOG_ERR, QString("Error opening audio device (%1)")
                         .arg(m_mainDevice));
        }
        if (m_audioFd < 0)
            usleep(50);
    }

    if (m_audioFd == -1)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error opening audio device (%1)").arg(m_mainDevice));
        return false;
    }

    fcntl(m_audioFd, F_SETFL, fcntl(m_audioFd, F_GETFL) & ~O_NONBLOCK);

    bool err = false;
    int  format;

    switch (m_outputFormat)
    {
        case FORMAT_U8:  format = AFMT_U8;      break;
        case FORMAT_S16: format = AFMT_S16_NE;  break;
        default:
            LOG(VB_GENERAL, LOG_ERR, QString("Unknown sample format: %1").arg(m_outputFormat));
            close(m_audioFd);
            m_audioFd = -1;
            return false;
    }

#if defined(AFMT_AC3) && defined(SNDCTL_DSP_GETFMTS)
    if (m_passthrough)
    {
        int format_support = 0;
        if (!ioctl(m_audioFd, SNDCTL_DSP_GETFMTS, &format_support) &&
            (format_support & AFMT_AC3))
        {
            format = AFMT_AC3;
        }
    }
#endif

    if (m_channels > 2)
    {
        if (ioctl(m_audioFd, SNDCTL_DSP_CHANNELS, &m_channels) < 0 ||
            ioctl(m_audioFd, SNDCTL_DSP_SPEED, &m_samplerate) < 0  ||
            ioctl(m_audioFd, SNDCTL_DSP_SETFMT, &format) < 0)
            err = true;
    }
    else
    {
        int stereo = m_channels - 1;
        if (ioctl(m_audioFd, SNDCTL_DSP_STEREO, &stereo) < 0     ||
            ioctl(m_audioFd, SNDCTL_DSP_SPEED, &m_samplerate) < 0  ||
            ioctl(m_audioFd, SNDCTL_DSP_SETFMT, &format) < 0)
            err = true;
    }

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Unable to set audio device (%1) to %2 kHz, %3 bits, "
                         "%4 channels")
                     .arg(m_mainDevice).arg(m_samplerate)
                     .arg(AudioOutputSettings::FormatToBits(m_outputFormat))
                     .arg(m_channels));

        close(m_audioFd);
        m_audioFd = -1;
        return false;
    }

    audio_buf_info info;
    ioctl(m_audioFd, SNDCTL_DSP_GETOSPACE, &info);
    // align by frame size
    m_fragmentSize = info.fragsize - (info.fragsize % m_outputBytesPerFrame);

    m_soundcardBufferSize = info.bytes;

    int caps;

    if (ioctl(m_audioFd, SNDCTL_DSP_GETCAPS, &caps) == 0)
    {
        if (!(caps & DSP_CAP_REALTIME))
            LOG(VB_GENERAL, LOG_WARNING, "The audio device cannot report buffer state "
                   "accurately! audio/video sync will be bad, continuing...");
    }
    else
        LOG(VB_GENERAL, LOG_ERR, "Unable to get audio card capabilities");

    // Setup volume control
    if (m_internalVolumeControl)
        VolumeInit();

    // Device opened successfully
    return true;
}

void AudioOutputOSS::CloseDevice()
{
    if (m_audioFd != -1)
        close(m_audioFd);

    m_audioFd = -1;

    VolumeCleanup();
}


void AudioOutputOSS::WriteAudio(uchar *aubuf, int size)
{
    if (m_audioFd < 0)
        return;

    uchar *tmpbuf;
    int written = 0, lw = 0;

    tmpbuf = aubuf;

    while ((written < size) &&
           ((lw = write(m_audioFd, tmpbuf, size - written)) > 0))
    {
        written += lw;
        tmpbuf += lw;
    }

    if (lw < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error writing to audio device (%1)")
                     .arg(m_mainDevice));
        return;
    }
}


int AudioOutputOSS::GetBufferedOnSoundcard(void) const
{
    int soundcard_buffer=0;
//GREG This is needs to be fixed for sure!
#ifdef SNDCTL_DSP_GETODELAY
    ioctl(m_audioFd, SNDCTL_DSP_GETODELAY, &soundcard_buffer); // bytes
#endif
    return soundcard_buffer;
}

void AudioOutputOSS::VolumeInit()
{
    m_mixerFd = -1;

    QString device = gLocalContext->GetSetting("MixerDevice", QString("/dev/mixer"));
    if (device.toLower() == "software")
        return;

    QByteArray dev = device.toAscii();
    m_mixerFd = open(dev.constData(), O_RDONLY);

    QString controlLabel = gLocalContext->GetSetting("MixerControl", QString("PCM"));

    if (controlLabel == "Master")
        m_control = SOUND_MIXER_VOLUME;
    else
        m_control = SOUND_MIXER_PCM;

    if (m_mixerFd < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Unable to open mixer: '%1'").arg(device));
        return;
    }

    if (m_setInitialVolume)
    {
        int tmpVol;
        int volume = gLocalContext->GetSetting("MasterMixerVolume", 80);
        tmpVol = (volume << 8) + volume;
        int ret = ioctl(m_mixerFd, MIXER_WRITE(SOUND_MIXER_VOLUME), &tmpVol);
        if (ret < 0)
            LOG(VB_GENERAL, LOG_ERR, QString("Error Setting initial Master Volume") + ENO);

        volume = gLocalContext->GetSetting("PCMMixerVolume", 80);
        tmpVol = (volume << 8) + volume;
        ret = ioctl(m_mixerFd, MIXER_WRITE(SOUND_MIXER_PCM), &tmpVol);
        if (ret < 0)
            LOG(VB_GENERAL, LOG_ERR, QString("Error setting initial PCM Volume") + ENO);
    }
}

void AudioOutputOSS::VolumeCleanup()
{
    if (m_mixerFd >= 0)
    {
        close(m_mixerFd);
        m_mixerFd = -1;
    }
}

int AudioOutputOSS::GetVolumeChannel(int channel) const
{
    int volume=0;
    int tmpVol=0;

    if (m_mixerFd <= 0)
        return 100;

    int ret = ioctl(m_mixerFd, MIXER_READ(m_control), &tmpVol);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error reading volume for channel %1").arg(channel));
        return 0;
    }

    if (channel == 0)
        volume = tmpVol & 0xff; // left
    else if (channel == 1)
        volume = (tmpVol >> 8) & 0xff; // right
    else
        LOG(VB_GENERAL, LOG_ERR, "Invalid channel. Only stereo volume supported");

    return volume;
}

void AudioOutputOSS::SetVolumeChannel(int channel, int volume)
{
    if (channel > 1)
    {
        // Don't support more than two channels!
        LOG(VB_GENERAL, LOG_ERR, QString("Error setting channel %1. Only 2 ch volume supported")
                .arg(channel));
        return;
    }

    if (volume > 100)
        volume = 100;
    if (volume < 0)
        volume = 0;

    if (m_mixerFd >= 0)
    {
        int tmpVol = 0;
        if (channel == 0)
            tmpVol = (GetVolumeChannel(1) << 8) + volume;
        else
            tmpVol = (volume << 8) + GetVolumeChannel(0);

        int ret = ioctl(m_mixerFd, MIXER_WRITE(m_control), &tmpVol);
        if (ret < 0)
            LOG(VB_GENERAL, LOG_ERR, QString("Error setting volume on channel %1").arg(channel));
    }
}
