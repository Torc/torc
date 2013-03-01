// Torc
#include "torclogging.h"
#include "audiooutputwin.h"

#include <iostream>
using namespace std;
#include <windows.h>
#include <mmsystem.h>
#include <initguid.h>

#ifndef WAVE_FORMAT_IEEE_FLOAT
#define WAVE_FORMAT_IEEE_FLOAT 0x0003
#endif

#ifndef WAVE_FORMAT_DOLBY_AC3_SPDIF
#define WAVE_FORMAT_DOLBY_AC3_SPDIF 0x0092
#endif

#ifndef WAVE_FORMAT_EXTENSIBLE
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE
#endif

#ifndef _WAVEFORMATEXTENSIBLE_
typedef struct {
    WAVEFORMATEX    Format;
    union {
        WORD wValidBitsPerSample;       // bits of precision
        WORD wSamplesPerBlock;          // valid if wBitsPerSample==0
        WORD wReserved;                 // If neither applies, set to zero
    } Samples;
    DWORD           dwChannelMask;      // which channels are present in stream
    GUID            SubFormat;
} WAVEFORMATEXTENSIBLE, *PWAVEFORMATEXTENSIBLE;
#endif

DEFINE_GUID(TORC1_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, WAVE_FORMAT_IEEE_FLOAT, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(TORC1_KSDATAFORMAT_SUBTYPE_PCM, WAVE_FORMAT_PCM, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(TORC1_KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF, WAVE_FORMAT_DOLBY_AC3_SPDIF, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

const uint AudioOutputWin::kPacketCount = 4;

class AudioOutputWinPrivate
{
  public:
    AudioOutputWinPrivate()
      : m_waveOut(NULL),
        m_waveHdrs(NULL),
        m_event(NULL)
    {
        m_waveHdrs = new WAVEHDR[AudioOutputWin::kPacketCount];
        memset(m_waveHdrs, 0, sizeof(WAVEHDR) * AudioOutputWin::kPacketCount);
        m_event = CreateEvent(NULL, FALSE, TRUE, NULL);
    }

    ~AudioOutputWinPrivate()
    {
        if (m_waveHdrs)
        {
            delete[] m_waveHdrs;
            m_waveHdrs = NULL;
        }

        if (m_event)
        {
            CloseHandle(m_event);
            m_event = NULL;
        }
    }

    void Close(void)
    {
        if (m_waveOut)
        {
            waveOutReset(m_waveOut);
            waveOutClose(m_waveOut);
            m_waveOut = NULL;
        }
    }

    static void CALLBACK WaveOutCallback(HWAVEOUT Hwo, UINT Message, DWORD Instance,
                                     DWORD Param1, DWORD Param2)
    {
        (void)Hwo;
        (void)Param1;
        (void)Param2;

        if (Message != WOM_DONE)
            return;

        AudioOutputWin *instance = reinterpret_cast<AudioOutputWin*>(Instance);
        InterlockedDecrement(&instance->m_packetCount);
        if (instance->m_packetCount < (int)AudioOutputWin::kPacketCount)
            SetEvent(instance->m_priv->m_event);
    }

  public:
    HWAVEOUT  m_waveOut;
    WAVEHDR  *m_waveHdrs;
    HANDLE    m_event;
};

AudioOutputWin::AudioOutputWin(const AudioSettings &Settings, AudioWrapper *Parent) :
    AudioOutput(Settings, Parent),
    m_priv(new AudioOutputWinPrivate()),
    m_packetCount(0),
    m_currentPacket(0),
    m_packetBuffer(NULL),
    m_useSPDIF(Settings.m_usePassthrough)
{
    InitSettings(Settings);
    if (Settings.m_openOnInit)
        Reconfigure(Settings);

    m_packetBuffer = (unsigned char**) calloc(kPacketCount, sizeof(unsigned char*));
}

AudioOutputWin::~AudioOutputWin()
{
    KillAudio();

    if (m_priv)
    {
        delete m_priv;
        m_priv = NULL;
    }

    if (m_packetBuffer)
    {
        for (uint i = 0; i < kPacketCount; i++)
            if (m_packetBuffer[i])
                free(m_packetBuffer[i]);

        free(m_packetBuffer);
        m_packetBuffer = NULL;
    }
}

AudioOutputSettings* AudioOutputWin::GetOutputSettings(bool /*digital*/)
{
    AudioOutputSettings *settings = new AudioOutputSettings();

    // We use WAVE_MAPPER to find a compatible device, so just claim support
    // for all of the standard rates
    QList<int> rates = settings->GetRates();
    foreach (int rate, rates)
        settings->AddSupportedRate(rate);

    // Support all standard formats
    settings->AddSupportedFormat(FORMAT_U8);
    settings->AddSupportedFormat(FORMAT_S16);
    settings->AddSupportedFormat(FORMAT_S24);
    settings->AddSupportedFormat(FORMAT_S32);
    settings->AddSupportedFormat(FORMAT_FLT);

    // Guess that we can do up to 5.1
    for (uint i = 2; i < 7; i++)
        settings->AddSupportedChannels(i);

    settings->SetPassthrough(PassthroughUnknown); //Maybe passthrough

    return settings;
}

bool AudioOutputWin::OpenDevice(void)
{
    CloseDevice();

    // fragments are 50ms worth of samples
    m_fragmentSize = 50 * m_outputBytesPerFrame * m_samplerate / 1000;
    // DirectSound buffer holds 4 fragments = 200ms worth of samples
    m_soundcardBufferSize = kPacketCount * m_fragmentSize;

    LOG(VB_AUDIO, LOG_INFO, QString("Buffering %1 fragments of %2 bytes each, total: %3 bytes")
            .arg(kPacketCount).arg(m_fragmentSize).arg(m_soundcardBufferSize));

    m_useSPDIF = m_passthrough || m_encode;

    WAVEFORMATEXTENSIBLE wf;
    wf.Format.nChannels            = m_channels;
    wf.Format.nSamplesPerSec       = m_samplerate;
    wf.Format.nBlockAlign          = m_outputBytesPerFrame;
    wf.Format.nAvgBytesPerSec      = m_samplerate * m_outputBytesPerFrame;
    wf.Format.wBitsPerSample       = (m_outputBytesPerFrame << 3) / m_channels;
    wf.Samples.wValidBitsPerSample = AudioOutputSettings::FormatToBits(m_outputFormat);

    if (m_useSPDIF)
    {
        wf.Format.wFormatTag = WAVE_FORMAT_DOLBY_AC3_SPDIF;
        wf.SubFormat         = TORC1_KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF;
    }
    else if (m_outputFormat == FORMAT_FLT)
    {
        wf.Format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        wf.SubFormat         = TORC1_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    }
    else
    {
        wf.Format.wFormatTag = WAVE_FORMAT_PCM;
        wf.SubFormat         = TORC1_KSDATAFORMAT_SUBTYPE_PCM;
    }

    LOG(VB_AUDIO, LOG_INFO, QString("New format: %1bits %2ch %3Hz %4")
            .arg(wf.Samples.wValidBitsPerSample).arg(m_channels)
            .arg(m_samplerate).arg(m_useSPDIF ? "data" : "PCM"));

    /* Only use the new WAVE_FORMAT_EXTENSIBLE format for multichannel audio */
    if (m_channels <= 2)
        wf.Format.cbSize = 0;
    else
    {
        wf.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wf.dwChannelMask = 0x003F; // 0x003F = 5.1 channels
        wf.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    }

    MMRESULT result = waveOutOpen(&m_priv->m_waveOut, WAVE_MAPPER,
                                  (WAVEFORMATEX *)&wf,
                                  (DWORD)AudioOutputWinPrivate::WaveOutCallback,
                                  (DWORD)this, CALLBACK_FUNCTION);

    if (result == WAVERR_BADFORMAT)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Unable to set audio output parameters %1").arg(wf.Format.nSamplesPerSec));
        return false;
    }

    return true;
}

void AudioOutputWin::CloseDevice(void)
{
    m_priv->Close();
}

void AudioOutputWin::WriteAudio(unsigned char *Buffer, int Size)
{
    if (!Size)
        return;

    if (InterlockedIncrement(&m_packetCount) > (int)kPacketCount)
    {
        while (m_packetCount > (int)kPacketCount)
            WaitForSingleObject(m_priv->m_event, INFINITE);
    }

    if (m_currentPacket >= kPacketCount)
        m_currentPacket = 0;

    WAVEHDR *wh = &m_priv->m_waveHdrs[m_currentPacket];
    if (wh->dwFlags & WHDR_PREPARED)
        waveOutUnprepareHeader(m_priv->m_waveOut, wh, sizeof(WAVEHDR));

    m_packetBuffer[m_currentPacket] = (unsigned char*)realloc(m_packetBuffer[m_currentPacket], Size);
    memcpy(m_packetBuffer[m_currentPacket], Buffer, Size);

    memset(wh, 0, sizeof(WAVEHDR));
    wh->lpData = (LPSTR)m_packetBuffer[m_currentPacket];
    wh->dwBufferLength = Size;

    if (MMSYSERR_NOERROR != waveOutPrepareHeader(m_priv->m_waveOut, wh, sizeof(WAVEHDR)))
        LOG(VB_GENERAL, LOG_ERR, "WriteAudio: failed to prepare header");
    else if (MMSYSERR_NOERROR != waveOutWrite(m_priv->m_waveOut, wh, sizeof(WAVEHDR)))
        LOG(VB_GENERAL, LOG_ERR, "WriteAudio: failed to write packet");

    m_currentPacket++;
}

int AudioOutputWin::GetBufferedOnSoundcard(void) const
{
    return m_packetCount * m_fragmentSize;
}

int AudioOutputWin::GetVolumeChannel(int channel) const
{
    DWORD winvolume = 0xffffffff;
    int Volume = 100;
    if (MMSYSERR_NOERROR == waveOutGetVolume((HWAVEOUT)WAVE_MAPPER, &winvolume))
        Volume = (channel == 0) ? (LOWORD(winvolume) / (0xffff / 100)) : (HIWORD(winvolume) / (0xffff / 100));

    LOG(VB_AUDIO, LOG_INFO, QString("GetVolume(%1) %2 (%3)").arg(channel).arg(Volume).arg(winvolume));
    return Volume;
}

void AudioOutputWin::SetVolumeChannel(int Channel, int Volume)
{
    if (Channel > 1)
        LOG(VB_GENERAL, LOG_ERR, "Windows volume only supports stereo!");

    DWORD winvolume = 0xffffffff;
    if (MMSYSERR_NOERROR == waveOutGetVolume((HWAVEOUT)WAVE_MAPPER, &winvolume))
    {
        if (Channel == 0)
            winvolume = (winvolume & 0xffff0000) | (Volume * (0xffff / 100));
        else
            winvolume = (winvolume & 0xffff) | ((Volume * (0xffff / 100)) << 16);
    }
    else
    {
        winvolume = Volume * (0xffff / 100);
        winvolume |= (winvolume << 16);
    }

    LOG(VB_AUDIO, LOG_INFO, QString("SetVolume(%1) %2(%3)").arg(Channel).arg(Volume).arg(winvolume));

    waveOutSetVolume((HWAVEOUT)WAVE_MAPPER, winvolume);
}

class AudioFactoryWin : public AudioFactory
{
    void Score(const AudioSettings &Settings, AudioWrapper *Parent, int &Score)
    {
        (void)Parent;
        bool match = Settings.m_mainDevice.startsWith("windows", Qt::CaseInsensitive);
        int score  =  match ? AUDIO_PRIORITY_MATCH : AUDIO_PRIORITY_LOW;
        if (Score <= score)
            Score = score;
    }

    AudioOutput* Create(const AudioSettings &Settings, AudioWrapper *Parent, int &Score)
    {
        bool match = Settings.m_mainDevice.startsWith("windows", Qt::CaseInsensitive);
        int score  =  match ? AUDIO_PRIORITY_MATCH : AUDIO_PRIORITY_LOW;
        if (Score <= score)
            return new AudioOutputWin(Settings, Parent);
        return NULL;
    }

    void GetDevices(QList<AudioDeviceConfig> &DeviceList)
    {
        AudioDeviceConfig *config = AudioOutput::GetAudioDeviceConfig(QString("Windows:Default"), QString("Default windows device"));
        if (config)
        {
            DeviceList.append(*config);
            delete config;
        }
    }
} AudioFactoryWin;
