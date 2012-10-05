// Qt
#include <QFile>

// Torc
#include "torclocalcontext.h"
#include "audiooutputalsa.h"

// redefine assert as no-op to quiet some compiler warnings
// about assert always evaluating true in alsa headers.
#undef assert
#define assert(x)

#define CHANNELS_MIN 1
#define CHANNELS_MAX 8

#define OPEN_FLAGS (SND_PCM_NO_AUTO_RESAMPLE|SND_PCM_NO_AUTO_FORMAT|    \
                    SND_PCM_NO_AUTO_CHANNELS)

#define FILTER_FLAGS ~(SND_PCM_NO_AUTO_FORMAT)

#define AERROR(str)   LOG(VB_GENERAL, LOG_ERR, str + QString(": %1").arg(snd_strerror(err)))
#define CHECKERR(str) { if (err < 0) { AERROR(str); return err; } }

AudioOutputALSA::AudioOutputALSA(const AudioSettings &Settings) :
    AudioOutputBase(Settings),
    m_pcmHandle(NULL),
    m_preallocBufferSize(-1),
    m_card(-1),
    m_device(-1),
    m_subdevice(-1)
{
    m_mixer.handle = NULL;
    m_mixer.elem = NULL;

    // Set everything up
    if (m_passthroughDevice == "auto")
    {
        m_passthroughDevice = m_mainDevice;

        int len = m_passthroughDevice.length();
        int args = m_passthroughDevice.indexOf(":");

            /*
             * AES description:
             * AES0=6 AES1=0x82 AES2=0x00 AES3=0x01.
             * AES0 = NON_AUDIO | PRO_MODE
             * AES1 = original stream, original PCM coder
             * AES2 = source and channel unspecified
             * AES3 = sample rate unspecified
             */
        bool s48k = gLocalContext->GetSetting("SPDIFRateOverride", false);
        QString iecarg = QString("AES0=6,AES1=0x82,AES2=0x00")  + (s48k ? QString() : QString(",AES3=0x01"));
        QString iecarg2 = QString("AES0=6 AES1=0x82 AES2=0x00") + (s48k ? QString() : QString(" AES3=0x01"));

        if (args < 0)
        {
            /* no existing parameters: add it behind device name */
            m_passthroughDevice += ":" + iecarg;
        }
        else
        {
            do
                ++args;
            while (args < m_passthroughDevice.length() &&
                   m_passthroughDevice[args].isSpace());
            if (args == m_passthroughDevice.length())
            {
                /* ":" but no parameters */
                m_passthroughDevice += iecarg;
            }
            else if (m_passthroughDevice[args] != '{')
            {
                /* a simple list of parameters: add it at the end of the list */
                m_passthroughDevice += "," + iecarg;
            }
            else
            {
                /* parameters in config syntax: add it inside the { } block */
                do
                    --len;
                while (len > 0 && m_passthroughDevice[len].isSpace());
                if (m_passthroughDevice[len] == '}')
                    m_passthroughDevice =
                        m_passthroughDevice.insert(len, " " + iecarg2);
            }
        }
    }
    else if (m_passthroughDevice.toLower() == "default")
    {
        m_passthroughDevice = m_mainDevice;
    }
    else
    {
        m_discreteDigital = true;
    }

    InitSettings(Settings);
    if (Settings.m_openOnInit)
        Reconfigure(Settings);
}

AudioOutputALSA::~AudioOutputALSA()
{
    KillAudio();
}

int AudioOutputALSA::TryOpenDevice(int Mode, int TryAC3)
{
    QByteArray device;
    int error = -1;

    if (TryAC3)
    {
        device = m_passthroughDevice.toAscii();
        LOG(VB_AUDIO, LOG_INFO, QString("OpenDevice %1 for passthrough").arg(m_passthroughDevice));
        error = snd_pcm_open(&m_pcmHandle, device.constData(), SND_PCM_STREAM_PLAYBACK, Mode);

        m_lastdevice = m_passthroughDevice;

        if (m_discreteDigital)
            return error;

        if (error < 0)
        {
            LOG(VB_AUDIO, LOG_INFO, QString("Auto setting passthrough failed (%1), defaulting "
                            "to main device").arg(snd_strerror(error)));
        }
    }

    if (!TryAC3 || error < 0)
    {
        // passthrough open failed, retry default device
        LOG(VB_AUDIO, LOG_INFO, QString("OpenDevice %1").arg(m_mainDevice));
        device = m_mainDevice.toAscii();
        error = snd_pcm_open(&m_pcmHandle, device.constData(),
                           SND_PCM_STREAM_PLAYBACK, Mode);
        m_lastdevice = m_mainDevice;
    }

    return error;
}

int AudioOutputALSA::GetPCMInfo(int &Card, int &Device, int &Subdevice)
{
    // Check for saved values
    if (m_card != -1 && m_device != -1 && m_subdevice != -1)
    {
        Card = m_card;
        Device = m_device;
        Subdevice = m_subdevice;
        return 0;
    }

    if (!m_pcmHandle)
        return -1;

    int err;
    snd_pcm_info_t *pcm_info = NULL;
    int tcard, tdevice, tsubdevice;

    snd_pcm_info_alloca(&pcm_info);

    err = snd_pcm_info(m_pcmHandle, pcm_info);
    CHECKERR("snd_pcm_info");

    err = snd_pcm_info_get_card(pcm_info);
    CHECKERR("snd_pcm_info_get_card");
    tcard = err;

    err = snd_pcm_info_get_device(pcm_info);
    CHECKERR("snd_pcm_info_get_device");
    tdevice = err;

    err = snd_pcm_info_get_subdevice(pcm_info);
    CHECKERR("snd_pcm_info_get_subdevice");
    tsubdevice = err;

    m_card = Card = tcard;
    m_device = Device = tdevice;
    m_subdevice = Subdevice = tsubdevice;

    return 0;
 }

bool AudioOutputALSA::IncPreallocBufferSize(int Requested, int BufferTime)
{
    int card, device, subdevice;

    m_preallocBufferSize = 0;

    if (GetPCMInfo(card, device, subdevice) < 0)
        return false;

    const QString pf = QString("/proc/asound/card%1/pcm%2p/sub%3/prealloc")
                       .arg(card).arg(device).arg(subdevice);

    QFile pfile(pf);
    QFile mfile(pf + "_max");

    if (!pfile.open(QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error opening %1. Fix reading permissions.").arg(pf));
        return false;
    }

    if (!mfile.open(QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Error opening %1").arg(pf + "_max"));
        return false;
    }

    int cur  = pfile.readAll().trimmed().toInt();
    int max  = mfile.readAll().trimmed().toInt();

    int size = ((int)(cur * (float)Requested / (float)BufferTime)
                / 64 + 1) * 64;

    LOG(VB_AUDIO, LOG_INFO, QString("Hardware audio buffer cur: %1 need: %2 max allowed: %3")
            .arg(cur).arg(size).arg(max));

    if (cur == max)
    {
        // It's already the maximum it can be, no point trying further
        pfile.close();
        mfile.close();
        return false;
    }

    if (size > max || !size)
        size = max;

    pfile.close();
    mfile.close();

    LOG(VB_GENERAL, LOG_ERR, QString("Try to manually increase audio buffer with: echo %1 "
                    "| sudo tee %2").arg(size).arg(pf));
    return false;
}

QByteArray AudioOutputALSA::GetELD(int Card, int Device, int Subdevice)
{
    QByteArray result;

    snd_hctl_t *hctl;
    snd_ctl_elem_info_t *info;
    snd_ctl_elem_id_t *id;
    snd_ctl_elem_value_t *control;
    snd_ctl_elem_type_t type;
    unsigned int count;
    int err;

    snd_ctl_elem_info_alloca(&info);
    snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_value_alloca(&control);


    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_PCM);
    snd_ctl_elem_id_set_name(id, "ELD");
    snd_ctl_elem_id_set_device(id, Device);

    if ((err = snd_hctl_open(&hctl,
                             QString("hw:%1").arg(Card).toAscii().constData(),
                             0)) < 0)
    {
        LOG(VB_AUDIO, LOG_INFO, QString("Control %1 open error: %2")
                .arg(Card)
                .arg(snd_strerror(err)));
        return result;
    }

    if ((err = snd_hctl_load(hctl)) < 0)
    {
        LOG(VB_AUDIO, LOG_INFO, QString("Control %1 load error: %2")
                .arg(Card)
                .arg(snd_strerror(err)));
        /* frees automatically the control which cannot be added. */
        return result;
    }

    snd_hctl_elem_t *elem = snd_hctl_find_elem(hctl, id);
    if (elem)
    {
        if ((err = snd_hctl_elem_info(elem, info)) < 0)
        {
            LOG(VB_AUDIO, LOG_INFO, QString("Control %1 snd_hctl_elem_info error: %2")
                    .arg(Card)
                    .arg(snd_strerror(err)));
            snd_hctl_close(hctl);
            return result;
        }

        count = snd_ctl_elem_info_get_count(info);
        type = snd_ctl_elem_info_get_type(info);
        if (!snd_ctl_elem_info_is_readable(info))
        {
            LOG(VB_AUDIO, LOG_INFO, QString("Control %1 element info is not readable")
                    .arg(Card));
            snd_hctl_close(hctl);
            return result;
        }

        if ((err = snd_hctl_elem_read(elem, control)) < 0)
        {
            LOG(VB_AUDIO, LOG_INFO, QString("Control %1 element read error: %2")
                    .arg(Card)
                    .arg(snd_strerror(err)));
            snd_hctl_close(hctl);
            return result;
        }

        if (type != SND_CTL_ELEM_TYPE_BYTES)
        {
            LOG(VB_AUDIO, LOG_INFO, QString("Control %1 element is of the wrong type")
                    .arg(Card));
            snd_hctl_close(hctl);
            return result;
        }

        result = QByteArray((char *)snd_ctl_elem_value_get_bytes(control), count);
    }

    snd_hctl_close(hctl);
    return result;
}

AudioOutputSettings* AudioOutputALSA::GetOutputSettings(bool Passthrough)
{
    snd_pcm_hw_params_t *params;
    snd_pcm_format_t afmt = SND_PCM_FORMAT_UNKNOWN;
    int err;

    AudioOutputSettings *settings = new AudioOutputSettings();

    if (m_pcmHandle)
    {
        snd_pcm_close(m_pcmHandle);
        m_pcmHandle = NULL;
    }

    if ((err = TryOpenDevice(OPEN_FLAGS, Passthrough)) < 0)
    {
        AERROR(QString("snd_pcm_open(\"%1\")").arg(m_lastdevice));
        delete settings;
        return NULL;
    }

    snd_pcm_hw_params_alloca(&params);

    if ((err = snd_pcm_hw_params_any(m_pcmHandle, params)) < 0)
    {
        snd_pcm_close(m_pcmHandle);
        if ((err = TryOpenDevice(OPEN_FLAGS&FILTER_FLAGS, Passthrough)) < 0)
        {
            AERROR(QString("snd_pcm_open(\"%1\")").arg(m_lastdevice));
            delete settings;
            return NULL;
        }

        if ((err = snd_pcm_hw_params_any(m_pcmHandle, params)) < 0)
        {
            AERROR("No playback configurations available");
            snd_pcm_close(m_pcmHandle);
            m_pcmHandle = NULL;
            delete settings;
            return NULL;
        }

        Warn("Supported audio format detection will be inacurrate "
             "(using plugin?)");
    }

    QList<int> rates = settings->GetRates();
    foreach (int rate, rates)
        if(snd_pcm_hw_params_test_rate(m_pcmHandle, params, rate, 0) >= 0)
            settings->AddSupportedRate(rate);

    QList<AudioFormat> formats = settings->GetFormats();
    foreach (AudioFormat fmt, formats)
    {
        switch (fmt)
        {
            case FORMAT_U8:     afmt = SND_PCM_FORMAT_U8;    break;
            case FORMAT_S16:    afmt = SND_PCM_FORMAT_S16;   break;
            case FORMAT_S24LSB: afmt = SND_PCM_FORMAT_S24;   break;
            case FORMAT_S24:    afmt = SND_PCM_FORMAT_S32;   break;
            case FORMAT_S32:    afmt = SND_PCM_FORMAT_S32;   break;
            case FORMAT_FLT:    afmt = SND_PCM_FORMAT_FLOAT; break;
            default:            continue;
        }

        if (snd_pcm_hw_params_test_format(m_pcmHandle, params, afmt) >= 0)
            settings->AddSupportedFormat(fmt);
    }

    for (uint channels = CHANNELS_MIN; channels <= CHANNELS_MAX; channels++)
        if (snd_pcm_hw_params_test_channels(m_pcmHandle, params, channels) >= 0)
            settings->AddSupportedChannels(channels);

    int card, device, subdevice;
    if (GetPCMInfo(card, device, subdevice) >= 0)
    {
        // Check if we can retrieve ELD for this device
        QByteArray eld = GetELD(card, device, subdevice);
        if (!eld.isEmpty())
        {
            LOG(VB_AUDIO, LOG_INFO, QString("Successfully retrieved ELD data"));
            settings->SetELD(eld);
       }
    }
    else
    {
        LOG(VB_AUDIO, LOG_INFO, "Can't get card and device number");
    }

    snd_pcm_close(m_pcmHandle);
    m_pcmHandle = NULL;

    /* Check if name or description contains information
       to know if device can accept passthrough or not */
    QMap<QString, QString> alsadevs = GetDevices("pcm");
    while(1)
    {
        QString realdevice = ((Passthrough && m_discreteDigital) ?
                               m_passthroughDevice : m_mainDevice);

        QString desc = alsadevs.value(realdevice);

        settings->SetPassthrough(PassthroughYes);
        if (realdevice.contains("digital", Qt::CaseInsensitive) ||
            desc.contains("digital", Qt::CaseInsensitive))
            break;
        if (realdevice.contains("iec958", Qt::CaseInsensitive))
            break;
        if (realdevice.contains("spdif", Qt::CaseInsensitive))
            break;
        if (realdevice.contains("hdmi", Qt::CaseInsensitive))
            break;

        settings->SetPassthrough(PassthroughNo);
        // PulseAudio does not support passthrough
        if (realdevice.contains("pulse", Qt::CaseInsensitive) ||
            desc.contains("pulse", Qt::CaseInsensitive))
            break;
        if (realdevice.contains("analog", Qt::CaseInsensitive) ||
            desc.contains("analog", Qt::CaseInsensitive))
            break;
        if (realdevice.contains("surround", Qt::CaseInsensitive) ||
            desc.contains("surround", Qt::CaseInsensitive))
            break;

        settings->SetPassthrough(PassthroughUnknown); // maybe
        break;
    }

    return settings;
}

bool AudioOutputALSA::OpenDevice()
{
    snd_pcm_format_t format;
    uint buffer_time;
    uint period_time;
    int err;

    if (m_pcmHandle != NULL)
        CloseDevice();

    if ((err = TryOpenDevice(0, m_passthrough || m_encode)) < 0)
    {
        AERROR(QString("snd_pcm_open(\"%1\")").arg(m_lastdevice));
        if (m_pcmHandle)
            CloseDevice();
        return false;
    }

    switch (m_outputFormat)
    {
        case FORMAT_U8:     format = SND_PCM_FORMAT_U8;    break;
        case FORMAT_S16:    format = SND_PCM_FORMAT_S16;   break;
        case FORMAT_S24LSB: format = SND_PCM_FORMAT_S24;   break;
        case FORMAT_S24:    format = SND_PCM_FORMAT_S32;   break;
        case FORMAT_S32:    format = SND_PCM_FORMAT_S32;   break;
        case FORMAT_FLT:    format = SND_PCM_FORMAT_FLOAT; break;
        default:
            LOG(VB_GENERAL, LOG_ERR, QString("Unknown sample format: %1").arg(m_outputFormat));
            return false;
    }

    // buffer 0.5s worth of samples
    buffer_time = gLocalContext->GetSetting("ALSABufferOverride", 500) * 1000;

    period_time = 4; // aim for an interrupt every (1/4th of buffer_time)

    err = SetParameters(m_pcmHandle, format, m_channels, m_samplerate,
                        buffer_time, period_time);
    if (err < 0)
    {
        AERROR("Unable to set ALSA parameters");
        CloseDevice();
        return false;
    }

    if (m_internalVolumeControl && !OpenMixer())
        LOG(VB_GENERAL, LOG_ERR, "Unable to open audio mixer. Volume control disabled");

    // Device opened successfully
    return true;
}

void AudioOutputALSA::CloseDevice()
{
    if (m_mixer.handle)
        snd_mixer_close(m_mixer.handle);
    m_mixer.handle = NULL;
    if (m_pcmHandle)
    {
        snd_pcm_close(m_pcmHandle);
        m_pcmHandle = NULL;
    }
}

template <class AudioDataType>
static inline void _ReorderSmpteToAlsa(AudioDataType *buf, uint frames,
                                       uint extrach)
{
    AudioDataType tmpC, tmpLFE, *buf2;

    for (uint i = 0; i < frames; i++)
    {
        buf = buf2 = buf + 2;

        tmpC = *buf++;
        tmpLFE = *buf++;
        *buf2++ = *buf++;
        *buf2++ = *buf++;
        *buf2++ = tmpC;
        *buf2++ = tmpLFE;
        buf += extrach;
    }
}

static inline void ReorderSmpteToAlsa(void *buf, uint frames,
                                      AudioFormat format, uint extrach)
{
    switch(AudioOutputSettings::FormatToBits(format))
    {
        case  8: _ReorderSmpteToAlsa((uchar *)buf, frames, extrach); break;
        case 16: _ReorderSmpteToAlsa((short *)buf, frames, extrach); break;
        default: _ReorderSmpteToAlsa((int   *)buf, frames, extrach); break;
    }
}

void AudioOutputALSA::WriteAudio(unsigned char *Buffer, int Size)
{
    uchar *tmpbuf = Buffer;
    uint frames = Size / m_outputBytesPerFrame;

    if (m_pcmHandle == NULL)
    {
        LOG(VB_GENERAL, LOG_ERR, "WriteAudio() called with m_pcmHandle == NULL!");
        return;
    }

    //Audio received is in SMPTE channel order, reorder to ALSA unless passthru
    if (!m_passthrough && (m_channels  == 6 || m_channels == 8))
    {
        ReorderSmpteToAlsa(Buffer, frames, m_outputFormat, m_channels - 6);
    }

    LOG(VB_AUDIO | VB_TIMESTAMP, LOG_INFO,
            QString("WriteAudio: Preparing %1 bytes (%2 frames)")
            .arg(Size).arg(frames));

    while (frames > 0)
    {
        int lw = snd_pcm_writei(m_pcmHandle, tmpbuf, frames);

        if (lw >= 0)
        {
            if ((uint)lw < frames)
                LOG(VB_AUDIO, LOG_INFO, QString("WriteAudio: short write %1 bytes (ok)")
                        .arg(lw * m_outputBytesPerFrame));

            frames -= lw;
            tmpbuf += lw * m_outputBytesPerFrame; // bytes
            continue;
        }

        int err = lw;
        switch (err)
        {
            case -EPIPE:
                 if (snd_pcm_state(m_pcmHandle) == SND_PCM_STATE_XRUN)
                 {
                    LOG(VB_AUDIO, LOG_INFO, "WriteAudio: buffer underrun");
                    if ((err = snd_pcm_prepare(m_pcmHandle)) < 0)
                    {
                        AERROR("WriteAudio: unable to recover from xrun");
                        return;
                    }
                }
                break;

            case -ESTRPIPE:
                LOG(VB_AUDIO, LOG_INFO, "WriteAudio: device is suspended");
                while ((err = snd_pcm_resume(m_pcmHandle)) == -EAGAIN)
                    usleep(200);

                if (err < 0)
                {
                    LOG(VB_GENERAL, LOG_ERR, "WriteAudio: resume failed");
                    if ((err = snd_pcm_prepare(m_pcmHandle)) < 0)
                    {
                        AERROR("WriteAudio: unable to recover from suspend");
                        return;
                    }
                }
                break;

            case -EBADFD:
                LOG(VB_GENERAL, LOG_ERR,
                    QString("WriteAudio: device is in a bad state (state = %1)")
                    .arg(snd_pcm_state(m_pcmHandle)));
                return;

            default:
                AERROR(QString("WriteAudio: Write failed, state: %1, err")
                       .arg(snd_pcm_state(m_pcmHandle)));
                return;
        }
    }
}

int AudioOutputALSA::GetBufferedOnSoundcard(void) const
{
    if (m_pcmHandle == NULL)
    {
        LOG(VB_GENERAL, LOG_ERR, "getBufferedOnSoundcard() called with m_pcmHandle == NULL!");
        return 0;
    }

    snd_pcm_sframes_t delay = 0;

    /* Delay is the total delay from writing to the pcm until the samples
       hit the DAC - includes buffered samples and any fixed latencies */
    if (snd_pcm_delay(m_pcmHandle, &delay) < 0)
        return 0;

    snd_pcm_state_t state = snd_pcm_state(m_pcmHandle);

    if (state == SND_PCM_STATE_RUNNING || state == SND_PCM_STATE_DRAINING)
    {
        delay *= m_outputBytesPerFrame;
    }
    else
    {
        delay = 0;
    }

    return delay;
}

/**
 * Set the various ALSA audio parameters.
 * Returns:
 * < 0 : an error occurred
 * 0   : Succeeded
 * > 0 : Buffer timelength returned by ALSA which is less than what we asked for
 */
int AudioOutputALSA::SetParameters(snd_pcm_t *Handle, snd_pcm_format_t Format,
                                   uint Channels, uint Rate, uint BufferTime,
                                   uint PeriodTime)
{
    int err;
    snd_pcm_hw_params_t  *params;
    snd_pcm_sw_params_t  *swparams;
    snd_pcm_uframes_t     period_size, period_size_min, period_size_max;
    snd_pcm_uframes_t     buffer_size, buffer_size_min, buffer_size_max;

    LOG(VB_AUDIO, LOG_INFO, QString("SetParameters(format=%1, channels=%2, rate=%3, "
                    "buffer_time=%4, period_time=%5)")
            .arg(Format).arg(Channels).arg(Rate).arg(BufferTime)
            .arg(PeriodTime));

    if (Handle == NULL)
    {
        LOG(VB_GENERAL, LOG_ERR, "SetParameters() called with handle == NULL!");
        return -1;
    }

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_sw_params_alloca(&swparams);

    /* choose all parameters */
    err = snd_pcm_hw_params_any(Handle, params);
    CHECKERR("No playback configurations available");

    /* set the interleaved read/write format */
    err = snd_pcm_hw_params_set_access(Handle, params,
                                       SND_PCM_ACCESS_RW_INTERLEAVED);
    CHECKERR(QString("Interleaved RW audio not available"));

    /* set the sample format */
    err = snd_pcm_hw_params_set_format(Handle, params, Format);
    CHECKERR(QString("Sample format %1 not available").arg(Format));

    /* set the count of channels */
    err = snd_pcm_hw_params_set_channels(Handle, params, Channels);
    CHECKERR(QString("Channels count %1 not available").arg(Channels));

    /* set the stream rate */
    if (m_srcQuality == QUALITY_DISABLED)
    {
        err = snd_pcm_hw_params_set_rate_resample(Handle, params, 1);
        CHECKERR(QString("Resampling setup failed").arg(Rate));

        uint rrate = Rate;
        err = snd_pcm_hw_params_set_rate_near(Handle, params, &rrate, 0);
        CHECKERR(QString("Rate %1Hz not available for playback: %s").arg(Rate));

        if (rrate != Rate)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Rate doesn't match (requested %1Hz, got %2Hz)")
                    .arg(Rate).arg(err));
            return err;
        }
    }
    else
    {
        err = snd_pcm_hw_params_set_rate(Handle, params, Rate, 0);
        CHECKERR(QString("Samplerate %1 Hz not available").arg(Rate));
    }

    /* set the buffer time */
    err = snd_pcm_hw_params_get_buffer_size_min(params, &buffer_size_min);
    err = snd_pcm_hw_params_get_buffer_size_max(params, &buffer_size_max);
    err = snd_pcm_hw_params_get_period_size_min(params, &period_size_min, NULL);
    err = snd_pcm_hw_params_get_period_size_max(params, &period_size_max, NULL);
    LOG(VB_AUDIO, LOG_INFO, QString("Buffer size range from %1 to %2")
            .arg(buffer_size_min)
            .arg(buffer_size_max));
    LOG(VB_AUDIO, LOG_INFO, QString("Period size range from %1 to %2")
            .arg(period_size_min)
            .arg(period_size_max));

    /* set the buffer time */
    uint originalbuffertime = BufferTime;
    bool canincrease = true;
    err = snd_pcm_hw_params_set_buffer_time_near(Handle, params,
                                                 &BufferTime, NULL);
    if (err < 0)
    {
        int dir             = -1;
        unsigned int buftmp = BufferTime;
        int attempt         = 0;
        do
        {
            err = snd_pcm_hw_params_set_buffer_time_near(Handle, params,
                                                         &BufferTime, &dir);
            if (err < 0)
            {
                AERROR(QString("Unable to set buffer time to %1us, retrying")
                       .arg(BufferTime));
                    /*
                     * with some drivers, snd_pcm_hw_params_set_buffer_time_near
                     * only works once, if that's the case no point trying with
                     * different values
                     */
                if ((BufferTime <= 100000) ||
                    (attempt > 0 && BufferTime == buftmp))
                {
                    LOG(VB_GENERAL, LOG_ERR, "Couldn't set buffer time, giving up");
                    return err;
                }
                BufferTime -= 100000;
                canincrease  = false;
                attempt++;
            }
        }
        while (err < 0);
    }

    /* See if we need to increase the prealloc'd buffer size
       If buffer_time is too small we could underrun - make 10% difference ok */
    if (BufferTime * 1.10f < (float)originalbuffertime)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Requested %1us got %2 buffer time")
                .arg(originalbuffertime).arg(BufferTime));
        // We need to increase preallocated buffer size in the driver
        if (canincrease && m_preallocBufferSize < 0)
        {
            IncPreallocBufferSize(originalbuffertime, BufferTime);
        }
    }

    LOG(VB_AUDIO, LOG_INFO, QString("Buffer time = %1 us").arg(BufferTime));

    /* set the period time */
    err = snd_pcm_hw_params_set_periods_near(Handle, params,
                                             &PeriodTime, NULL);
    CHECKERR(QString("Unable to set period time %1").arg(PeriodTime));
    LOG(VB_AUDIO, LOG_INFO, QString("Period time = %1 periods").arg(PeriodTime));

    /* write the parameters to device */
    err = snd_pcm_hw_params(Handle, params);
    CHECKERR("Unable to set hw params for playback");

    err = snd_pcm_get_params(Handle, &buffer_size, &period_size);
    CHECKERR("Unable to get PCM params");
    LOG(VB_AUDIO, LOG_INFO, QString("Buffer size = %1 | Period size = %2")
            .arg(buffer_size).arg(period_size));

    /* set member variables */
    m_soundcardBufferSize = buffer_size * m_outputBytesPerFrame;
    m_fragmentSize = (period_size >> 1) * m_outputBytesPerFrame;

    /* get the current swparams */
    err = snd_pcm_sw_params_current(Handle, swparams);
    CHECKERR("Unable to get current swparams");

    /* start the transfer after period_size */
    err = snd_pcm_sw_params_set_start_threshold(Handle, swparams, period_size);
    CHECKERR("Unable to set start threshold");

    /* allow the transfer when at least period_size samples can be processed */
    err = snd_pcm_sw_params_set_avail_min(Handle, swparams, period_size);
    CHECKERR("Unable to set avail min");

    /* write the parameters to the playback device */
    err = snd_pcm_sw_params(Handle, swparams);
    CHECKERR("Unable to set sw params");

    err = snd_pcm_prepare(Handle);
    CHECKERR("Unable to prepare the PCM");

    return 0;
}

int AudioOutputALSA::GetVolumeChannel(int Channel) const
{
    int retvol = 0;

    if (!m_mixer.elem)
        return retvol;

    snd_mixer_selem_channel_id_t chan = (snd_mixer_selem_channel_id_t) Channel;
    if (!snd_mixer_selem_has_playback_channel(m_mixer.elem, chan))
        return retvol;

    long mixervol;
    int chk;
    if ((chk = snd_mixer_selem_get_playback_volume(m_mixer.elem,
                                                   chan,
                                                   &mixervol)) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("failed to get channel %1 volume, mixer %2/%3: %4")
                .arg(Channel).arg(m_mixer.device)
                .arg(m_mixer.control)
                .arg(snd_strerror(chk)));
    }
    else
    {
        retvol = (m_mixer.volrange != 0L) ? (mixervol - m_mixer.volmin) *
                                            100.0f / m_mixer.volrange + 0.5f
                                            : 0;
        retvol = std::max(retvol, 0);
        retvol = std::min(retvol, 100);
        LOG(VB_AUDIO, LOG_INFO, QString("get volume channel %1: %2")
                .arg(Channel).arg(retvol));
    }
    return retvol;
}

void AudioOutputALSA::SetVolumeChannel(int Channel, int Volume)
{
    if (!(m_internalVolumeControl && m_mixer.elem))
        return;

    long mixervol = Volume * m_mixer.volrange / 100.0f - m_mixer.volmin + 0.5f;
    mixervol = std::max(mixervol, m_mixer.volmin);
    mixervol = std::min(mixervol, m_mixer.volmax);

    snd_mixer_selem_channel_id_t chan = (snd_mixer_selem_channel_id_t) Channel;
    if (snd_mixer_selem_set_playback_volume(m_mixer.elem, chan, mixervol) < 0)
        LOG(VB_GENERAL, LOG_ERR, QString("failed to set channel %1 volume").arg(Channel));
    else
        LOG(VB_AUDIO, LOG_INFO, QString("channel %1 volume set %2 => %3")
                .arg(Channel).arg(Volume).arg(mixervol));
}

bool AudioOutputALSA::OpenMixer(void)
{
    if (!m_pcmHandle)
    {
        LOG(VB_GENERAL, LOG_ERR, "mixer setup without a pcm");
        return false;
    }
    m_mixer.device = gLocalContext->GetSetting("MixerDevice", "default");
    m_mixer.device = m_mixer.device.remove(QString("ALSA:"));
    if (m_mixer.device.toLower() == "software")
        return true;

    m_mixer.control = gLocalContext->GetSetting("MixerControl", "PCM");

    QString mixer_device_tag = QString("mixer device %1").arg(m_mixer.device);

    int chk;
    if ((chk = snd_mixer_open(&m_mixer.handle, 0)) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("failed to open mixer device %1: %2")
                .arg(mixer_device_tag).arg(snd_strerror(chk)));
        return false;
    }

    QByteArray dev_ba = m_mixer.device.toAscii();
    struct snd_mixer_selem_regopt regopts =
        {1, SND_MIXER_SABSTRACT_NONE, dev_ba.constData(), NULL, NULL};

    if ((chk = snd_mixer_selem_register(m_mixer.handle, &regopts, NULL)) < 0)
    {
        snd_mixer_close(m_mixer.handle);
        m_mixer.handle = NULL;
        LOG(VB_GENERAL, LOG_ERR, QString("failed to register %1: %2")
                .arg(mixer_device_tag).arg(snd_strerror(chk)));
        return false;
    }

    if ((chk = snd_mixer_load(m_mixer.handle)) < 0)
    {
        snd_mixer_close(m_mixer.handle);
        m_mixer.handle = NULL;
        LOG(VB_GENERAL, LOG_ERR, QString("failed to load %1: %2")
                .arg(mixer_device_tag).arg(snd_strerror(chk)));
        return false;
    }

    m_mixer.elem = NULL;
    uint elcount = snd_mixer_get_count(m_mixer.handle);
    snd_mixer_elem_t* elx = snd_mixer_first_elem(m_mixer.handle);

    for (uint ctr = 0; elx != NULL && ctr < elcount; ctr++)
    {
        QString tmp = QString(snd_mixer_selem_get_name(elx));
        if (m_mixer.control == tmp &&
            !snd_mixer_selem_is_enumerated(elx) &&
            snd_mixer_selem_has_playback_volume(elx) &&
            snd_mixer_selem_is_active(elx))
        {
            m_mixer.elem = elx;
            LOG(VB_AUDIO, LOG_INFO, QString("found playback control %1 on %2")
                    .arg(m_mixer.control)
                    .arg(mixer_device_tag));
            break;
        }
        elx = snd_mixer_elem_next(elx);
    }
    if (!m_mixer.elem)
    {
        snd_mixer_close(m_mixer.handle);
        m_mixer.handle = NULL;
        LOG(VB_GENERAL, LOG_ERR, QString("no playback control %1 found on %2")
                .arg(m_mixer.control).arg(mixer_device_tag));
        return false;
    }
    if ((snd_mixer_selem_get_playback_volume_range(m_mixer.elem,
                                                   &m_mixer.volmin,
                                                   &m_mixer.volmax) < 0))
    {
        snd_mixer_close(m_mixer.handle);
        m_mixer.handle = NULL;
        LOG(VB_GENERAL, LOG_ERR, QString("failed to get volume range on %1/%2")
                .arg(mixer_device_tag).arg(m_mixer.control));
        return false;
    }

    m_mixer.volrange = m_mixer.volmax - m_mixer.volmin;
    LOG(VB_AUDIO, LOG_INFO, QString("mixer volume range on %1/%2 - min %3, max %4, range %5")
            .arg(mixer_device_tag).arg(m_mixer.control)
            .arg(m_mixer.volmin).arg(m_mixer.volmax).arg(m_mixer.volrange));
    LOG(VB_AUDIO, LOG_INFO, QString("%1/%2 set up successfully")
            .arg(mixer_device_tag)
            .arg(m_mixer.control));

    if (m_setInitialVolume)
    {
        int initial_vol;
        if (m_mixer.control == "PCM")
            initial_vol = gLocalContext->GetSetting("PCMMixerVolume", 80);
        else
            initial_vol = gLocalContext->GetSetting("MasterMixerVolume", 80);
        for (int ch = 0; ch < m_channels; ++ch)
            SetVolumeChannel(ch, initial_vol);
    }

    return true;
}

QMap<QString, QString> AudioOutputALSA::GetDevices(const char* Type)
{
    QMap<QString, QString> alsadevs;

    // Loop through the sound cards to get Alsa device hints.
    // NB Don't use snd_device_name_hint(-1,..) since there is a potential
    // access violation using this ALSA API with libasound.so.2.0.0.
    // See http://code.google.com/p/chromium/issues/detail?id=95797
    int card = -1;
    while (!snd_card_next(&card) && card >= 0)
    {
        void** hints = NULL;
        int error = snd_device_name_hint(card, Type, &hints);
        if (error == 0)
        {
            void *hint = NULL;
            for (int i = 0; (hint = hints[i]) != NULL; ++i)
            {
                char *name = snd_device_name_get_hint(hint, "NAME");
                if (name)
                {
                    char *desc = snd_device_name_get_hint(hint, "DESC");
                    if (desc)
                    {
                        if (strcmp(name, "null"))
                            alsadevs.insert(name, desc);

                        free(desc);
                    }

                    free(name);
                }
            }

            snd_device_name_free_hint(hints);
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to get device hints for card %1, error %2")
                    .arg(card).arg(error));
        }
    }

    // Work around ALSA bug < 1.0.22 ; where snd_device_name_hint can corrupt
    // global ALSA memory context
#if SND_LIB_MAJOR == 1
#if SND_LIB_MINOR == 0
#if SND_LIB_SUBMINOR < 22
    snd_config_update_free_global();
#endif
#endif
#endif
    return alsadevs;
}
