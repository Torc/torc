// Torc
#include "torclogging.h"
#include "audiooutputnull.h"

#define CHANNELS_MIN 1
#define CHANNELS_MAX 8

AudioOutputNULL::AudioOutputNULL(const AudioSettings &Settings)
  : AudioOutputBase(Settings),
    m_pcmBufferLock(QMutex::NonRecursive),
    m_currentBufferSize(0)
{
    memset(m_pcmBuffer, 0, sizeof(char) * NULL_BUFFER_SIZE);
    InitSettings(Settings);
    if (Settings.m_openOnInit)
        Reconfigure(Settings);
}

AudioOutputNULL::~AudioOutputNULL()
{
    KillAudio();
}

bool AudioOutputNULL::OpenDevice(void)
{
    LOG(VB_GENERAL, LOG_INFO, "Opening NULL audio device, will fail.");

    m_fragmentSize = NULL_BUFFER_SIZE / 2;
    m_soundcardBufferSize = NULL_BUFFER_SIZE;

    return false;
}

void AudioOutputNULL::CloseDevice(void)
{
}

AudioOutputSettings* AudioOutputNULL::GetOutputSettings(bool Digital)
{
    (void)Digital;

    // Pretend that we support everything
    AudioOutputSettings *settings = new AudioOutputSettings();

    QList<int> rates = settings->GetRates();
    foreach (int rate, rates)
        if (rate > 0)
            settings->AddSupportedRate(rate);

    QList<AudioFormat> formats = settings->GetFormats();
    foreach (AudioFormat format, formats)
        settings->AddSupportedFormat(format);

    for (uint channels = CHANNELS_MIN; channels <= CHANNELS_MAX; channels++)
        settings->AddSupportedChannels(channels);

    settings->SetPassthrough(PassthroughNo);

    return settings;
}


void AudioOutputNULL::WriteAudio(unsigned char* Buffer, int Size)
{
    if (m_bufferOutputDataForUse)
    {
        if (Size + m_currentBufferSize > NULL_BUFFER_SIZE)
        {
            LOG(VB_GENERAL, LOG_ERR, "Buffer overrun");
            return;
        }

        m_pcmBufferLock.lock();
        memcpy(m_pcmBuffer + m_currentBufferSize, Buffer, Size);
        m_currentBufferSize += Size;
        m_pcmBufferLock.unlock();
    }
}

int AudioOutputNULL::ReadOutputData(unsigned char *ReadBuffer, int MaxLength)
{
    int read = MaxLength;
    if (read > m_currentBufferSize)
        read = m_currentBufferSize;

    m_pcmBufferLock.lock();
    memcpy(ReadBuffer, m_pcmBuffer, read);
    memmove(m_pcmBuffer, m_pcmBuffer + read,
            m_currentBufferSize - read);
    m_currentBufferSize -= read;
    m_pcmBufferLock.unlock();

    return read;
}

void AudioOutputNULL::Reset(void)
{
    if (m_bufferOutputDataForUse)
    {
        m_pcmBufferLock.lock();
        m_currentBufferSize = 0;
        m_pcmBufferLock.unlock();
    }

    AudioOutputBase::Reset();
}

int AudioOutputNULL::GetVolumeChannel(int Channel) const
{
    (void)Channel;
    return 100;
}

void AudioOutputNULL::SetVolumeChannel(int Channel, int Volume)
{
    (void)Channel;
    (void)Volume;
}

int AudioOutputNULL::GetBufferedOnSoundcard(void) const
{
    if (m_bufferOutputDataForUse)
        return m_currentBufferSize;
    return 0;
}

class AudioFactoryNULL : public AudioFactory
{
    void Score(const AudioSettings &Settings, int &Score)
    {
        bool match = Settings.m_mainDevice.startsWith("null", Qt::CaseInsensitive);
        int score  =  match ? AUDIO_PRIORITY_MATCH : AUDIO_PRIORITY_LOWEST;
        if (Score <= score)
            Score = score;
    }

    AudioOutput* Create(const AudioSettings &Settings, int &Score)
    {
        bool match = Settings.m_mainDevice.startsWith("null", Qt::CaseInsensitive);
        int score  =  match ? AUDIO_PRIORITY_MATCH : AUDIO_PRIORITY_LOWEST;
        if (Score <= score)
            return new AudioOutputNULL(Settings);
        return NULL;
    }

    void GetDevices(QList<AudioDeviceConfig> &DeviceList)
    {
        AudioDeviceConfig config(QString("NULL"), QString("Null device"));
        DeviceList.append(config);
    }
} AudioFactoryNULL;
