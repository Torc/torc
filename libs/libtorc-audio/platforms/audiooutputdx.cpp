// Torc
#include "torccoreutils.h"
#include "torccompat.h"
#include "torclogging.h"
#include "audiooutputdx.h"

using namespace std;
#include <iostream>
#include <cmath>
#include <mmsystem.h>
#include <initguid.h>

#include "include/dsound.h"

#ifndef WAVE_FORMAT_DOLBY_AC3_SPDIF
#define WAVE_FORMAT_DOLBY_AC3_SPDIF 0x0092
#endif

#ifndef WAVE_FORMAT_IEEE_FLOAT
#define WAVE_FORMAT_IEEE_FLOAT 0x0003
#endif

#ifndef WAVE_FORMAT_EXTENSIBLE
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE
#endif

#ifndef _WAVEFORMATEXTENSIBLE_
typedef struct {
    WAVEFORMATEX Format;
    union {
        WORD wValidBitsPerSample;       // bits of precision
        WORD wSamplesPerBlock;          // valid if wBitsPerSample==0
        WORD wReserved;                 // If neither applies, set to zero
    } Samples;
    DWORD        dwChannelMask;         // which channels are present in stream
    GUID         SubFormat;
} WAVEFORMATEXTENSIBLE, *PWAVEFORMATEXTENSIBLE;
#endif

DEFINE_GUID(TORC2_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, WAVE_FORMAT_IEEE_FLOAT, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(TORC2_KSDATAFORMAT_SUBTYPE_PCM, WAVE_FORMAT_PCM, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(TORC2_KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF, WAVE_FORMAT_DOLBY_AC3_SPDIF, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

typedef HRESULT (WINAPI *LPFNDSC) (LPGUID, LPDIRECTSOUND *, LPUNKNOWN);
typedef HRESULT (WINAPI *LPFNDSE) (LPDSENUMCALLBACK, LPVOID);

class AudioOutputDXPrivate
{
  public:
    AudioOutputDXPrivate(AudioOutputDX *Parent)
      : m_parent(Parent),
        m_directSoundLib(NULL),
        m_directSound(NULL),
        m_directSoundBuffer(NULL),
        m_playStarted(false),
        m_bufferPosition(0),
        m_chosedGUID(NULL),
        m_deviceCount(0),
        m_deviceNum(0)
    {
    }

    ~AudioOutputDXPrivate()
    {
        DestroyDSBuffer();

        if (m_directSound)
            IDirectSound_Release(m_directSound);

        if (m_directSoundLib)
           FreeLibrary(m_directSoundLib);
    }

    int InitDirectSound(bool Passthrough = false)
    {
        LPFNDSC OurDirectSoundCreate;
        LPFNDSE OurDirectSoundEnumerate;
        bool ok;

        ResetDirectSound();

        m_directSoundLib = LoadLibrary("DSOUND.DLL");
        if (m_directSoundLib == NULL)
        {
            LOG(VB_GENERAL, LOG_ERR, "Cannot open DSOUND.DLL");
            goto error;
        }

        if (m_parent)  // parent can be NULL only when called from GetDXDevices()
            m_deviceName = Passthrough ? m_parent->m_passthroughDevice : m_parent->m_mainDevice;
        m_deviceName = m_deviceName.section(':', 1);
        m_deviceNum  = m_deviceName.toInt(&ok, 10);

        LOG(VB_AUDIO, LOG_INFO, QString("Looking for device num:%1 or name:%2")
                .arg(m_deviceNum).arg(m_deviceName));

        OurDirectSoundEnumerate =
            (LPFNDSE)GetProcAddress(m_directSoundLib, "DirectSoundEnumerateA");

        if(OurDirectSoundEnumerate)
            if(FAILED(OurDirectSoundEnumerate(DSEnumCallback, this)))
                LOG(VB_GENERAL, LOG_ERR, "DirectSoundEnumerate FAILED");

        if (!m_chosedGUID)
        {
            m_deviceNum = 0;
            m_deviceName = "Primary Sound Driver";
        }

        LOG(VB_AUDIO, LOG_INFO, QString("Using device %1:%2").arg(m_deviceNum).arg(m_deviceName));

        OurDirectSoundCreate =
            (LPFNDSC)GetProcAddress(m_directSoundLib, "DirectSoundCreate");

        if (OurDirectSoundCreate == NULL)
        {
            LOG(VB_GENERAL, LOG_ERR, "GetProcAddress FAILED");
            goto error;
        }

        if (FAILED(OurDirectSoundCreate(m_chosedGUID, &m_directSound, NULL)))
        {
            LOG(VB_GENERAL, LOG_ERR, "Cannot create a direct sound device");
            goto error;
        }

        /* Set DirectSound Cooperative level, ie what control we want over Windows
         * sound device. In our case, DSSCL_EXCLUSIVE means that we can modify the
         * settings of the primary buffer, but also that only the sound of our
         * application will be hearable when it will have the focus.
         * !!! (this is not really working as intended yet because to set the
         * cooperative level you need the window handle of your application, and
         * I don't know of any easy way to get it. Especially since we might play
         * sound without any video, and so what window handle should we use ???
         * The hack for now is to use the Desktop window handle - it seems to be
         * working */
        if (FAILED(IDirectSound_SetCooperativeLevel(m_directSound, GetDesktopWindow(), DSSCL_EXCLUSIVE)))
            LOG(VB_GENERAL, LOG_ERR, "Cannot set DS cooperative level");

        LOG(VB_AUDIO, LOG_INFO, "Initialised DirectSound");

        return 0;

     error:
        m_directSound = NULL;
        if (m_directSoundLib)
        {
            FreeLibrary(m_directSoundLib);
            m_directSoundLib = NULL;
        }
        return -1;
    }

    void ResetDirectSound(void)
    {
        DestroyDSBuffer();

        if (m_directSound)
        {
            IDirectSound_Release(m_directSound);
            m_directSound = NULL;
        }

        if (m_directSoundLib)
        {
           FreeLibrary(m_directSoundLib);
           m_directSoundLib = NULL;
        }

        m_chosedGUID   = NULL;
        m_deviceCount = 0;
        m_deviceNum   = 0;
        m_deviceList.clear();
    }

    void DestroyDSBuffer(void)
    {
        if (!m_directSoundBuffer)
            return;

        LOG(VB_AUDIO, LOG_INFO, "Destroying DirectSound buffer");
        IDirectSoundBuffer_Stop(m_directSoundBuffer);
        m_bufferPosition = 0;
        IDirectSoundBuffer_SetCurrentPosition(m_directSoundBuffer, m_bufferPosition);
        m_playStarted = false;
        IDirectSoundBuffer_Release(m_directSoundBuffer);
        m_directSoundBuffer = NULL;
    }

    void FillBuffer(unsigned char *buffer, int size)
    {
        void    *p_write_position, *p_wrap_around;
        DWORD   l_bytes1, l_bytes2, play_pos, write_pos;
        HRESULT dsresult;

        if (!m_directSoundBuffer)
            return;

        while (true)
        {
            dsresult = IDirectSoundBuffer_GetCurrentPosition(m_directSoundBuffer,
                                                             &play_pos, &write_pos);
            if (dsresult != DS_OK)
            {
                LOG(VB_GENERAL, LOG_ERR, "Cannot get current buffer position");
                return;
            }

            LOG(VB_TIMESTAMP, LOG_INFO, QString("play: %1 write: %2 wcursor: %3").arg(play_pos).arg(write_pos).arg(m_bufferPosition));

            while ((m_bufferPosition < write_pos &&
                   ((m_bufferPosition >= play_pos && write_pos >= play_pos)   ||
                    (m_bufferPosition < play_pos  && write_pos <  play_pos))) ||
                   (m_bufferPosition >= play_pos && write_pos < play_pos))
            {
                LOG(VB_GENERAL, LOG_ERR, "buffer underrun");
                m_bufferPosition += size;
                while (m_bufferPosition >= (DWORD)m_parent->m_soundcardBufferSize)
                    m_bufferPosition -= m_parent->m_soundcardBufferSize;
            }

            if ((m_bufferPosition < play_pos  && m_bufferPosition + size >= play_pos) ||
                (m_bufferPosition >= play_pos &&
                 m_bufferPosition + size >= play_pos + m_parent->m_soundcardBufferSize))
            {
                TorcCoreUtils::USleep(50000);
                continue;
            }

            break;
        }

        dsresult = IDirectSoundBuffer_Lock(
                       m_directSoundBuffer,   /* DS buffer */
                       m_bufferPosition,       /* Start offset */
                       size,                  /* Number of bytes */
                       &p_write_position,     /* Address of lock start */
                       &l_bytes1,             /* Bytes locked before wrap */
                       &p_wrap_around,        /* Buffer address (if wrap around) */
                       &l_bytes2,             /* Count of bytes after wrap around */
                       0);                    /* Flags */

        if (dsresult == DSERR_BUFFERLOST)
        {
            IDirectSoundBuffer_Restore(m_directSoundBuffer);
            dsresult = IDirectSoundBuffer_Lock(m_directSoundBuffer, m_bufferPosition, size,
                                               &p_write_position, &l_bytes1,
                                               &p_wrap_around, &l_bytes2, 0);
        }

        if (dsresult != DS_OK)
        {
            LOG(VB_GENERAL, LOG_ERR, "Cannot lock buffer, audio dropped");
            return;
        }

        memcpy(p_write_position, buffer, l_bytes1);
        if (p_wrap_around)
            memcpy(p_wrap_around, buffer + l_bytes1, l_bytes2);

        m_bufferPosition += l_bytes1 + l_bytes2;

        while (m_bufferPosition >= (DWORD)m_parent->m_soundcardBufferSize)
            m_bufferPosition -= m_parent->m_soundcardBufferSize;

        IDirectSoundBuffer_Unlock(m_directSoundBuffer, p_write_position, l_bytes1,
                                  p_wrap_around, l_bytes2);
    }

    bool StartPlayback(void)
    {
        HRESULT dsresult;

        dsresult = IDirectSoundBuffer_Play(m_directSoundBuffer, 0, 0, DSBPLAY_LOOPING);
        if (dsresult == DSERR_BUFFERLOST)
        {
            IDirectSoundBuffer_Restore(m_directSoundBuffer);
            dsresult = IDirectSoundBuffer_Play(m_directSoundBuffer, 0, 0, DSBPLAY_LOOPING);
        }

        if (dsresult != DS_OK)
        {
            LOG(VB_GENERAL, LOG_ERR, "Cannot start playing buffer");
            m_playStarted = false;
            return false;
        }

        m_playStarted=true;
        return true;
    }

    static int CALLBACK DSEnumCallback(LPGUID lpGuid,LPCSTR lpcstrDesc, LPCSTR lpcstrModule, LPVOID lpContext)
    {
        const QString enum_desc = lpcstrDesc;
        AudioOutputDXPrivate *context = static_cast<AudioOutputDXPrivate*>(lpContext);
        const QString cfg_desc  = context->m_deviceName;
        const int m_deviceNum   = context->m_deviceNum;
        const int m_deviceCount = context->m_deviceCount;

        LOG(VB_AUDIO, LOG_INFO, QString("Device %1:" + enum_desc).arg(m_deviceCount));

        if ((m_deviceNum == m_deviceCount ||
            (m_deviceNum == 0 && !cfg_desc.isEmpty() &&
            enum_desc.startsWith(cfg_desc, Qt::CaseInsensitive))) && lpGuid)
        {
            context->m_deviceGUID = *lpGuid;
            context->m_chosedGUID = &(context->m_deviceGUID);
            context->m_deviceName = enum_desc;
            context->m_deviceNum  = m_deviceCount;
        }

        context->m_deviceList.insert(m_deviceCount, enum_desc);
        context->m_deviceCount++;
        return 1;
    }

  public:
    AudioOutputDX      *m_parent;
    HINSTANCE           m_directSoundLib;
    LPDIRECTSOUND       m_directSound;
    LPDIRECTSOUNDBUFFER m_directSoundBuffer;
    bool                m_playStarted;
    DWORD               m_bufferPosition;
    GUID                m_deviceGUID;
    GUID               *m_chosedGUID;
    int                 m_deviceCount;
    int                 m_deviceNum;
    QString             m_deviceName;
    QMap<int, QString>  m_deviceList;
};


AudioOutputDX::AudioOutputDX(const AudioSettings &Settings, AudioWrapper *Parent)
  : AudioOutput(Settings, Parent),
    m_priv(new AudioOutputDXPrivate(this)),
    m_useSPDIF(Settings.m_usePassthrough)
{
    timeBeginPeriod(1);
    InitSettings(Settings);

    if (m_passthroughDevice == "auto" || m_passthroughDevice.toLower() == "default")
        m_passthroughDevice = m_mainDevice;
    else
        m_discreteDigital = true;

    if (Settings.m_openOnInit)
        Reconfigure(Settings);
}

AudioOutputDX::~AudioOutputDX()
{
    KillAudio();

    delete m_priv;
    m_priv = NULL;

    timeEndPeriod(1);
}

AudioOutputSettings* AudioOutputDX::GetOutputSettings(bool Passthrough)
{
    AudioOutputSettings *settings = new AudioOutputSettings();
    DSCAPS devcaps;
    devcaps.dwSize = sizeof(DSCAPS);

    m_priv->InitDirectSound(Passthrough);
    if ((!m_priv->m_directSound || !m_priv->m_directSoundLib) || FAILED(IDirectSound_GetCaps(m_priv->m_directSound, &devcaps)) )
    {
        delete settings;
        return NULL;
    }

    LOG(VB_AUDIO, LOG_INFO, QString("GetCaps sample rate min: %1 max: %2")
            .arg(devcaps.dwMinSecondarySampleRate)
            .arg(devcaps.dwMaxSecondarySampleRate));

    /* We shouldn't assume we can do everything between min and max but
       there's no way to test individual rates, so we'll have to */
    QList<int> rates = settings->GetRates();
    foreach (int rate, rates)
    {
        DWORD drate = (DWORD)rate;
        if((drate >= devcaps.dwMinSecondarySampleRate) ||(drate <= devcaps.dwMaxSecondarySampleRate))
            settings->AddSupportedRate(rate);
    }

    /* We can only test for 8 and 16 bit support, DS uses float internally
       Guess that we can support S24 and S32 too */
    if (devcaps.dwFlags & DSCAPS_PRIMARY8BIT)
        settings->AddSupportedFormat(FORMAT_U8);
    if (devcaps.dwFlags & DSCAPS_PRIMARY16BIT)
        settings->AddSupportedFormat(FORMAT_S16);
    settings->AddSupportedFormat(FORMAT_S24);
    settings->AddSupportedFormat(FORMAT_S32);
    settings->AddSupportedFormat(FORMAT_FLT);

    /* No way to test anything other than mono or stereo, guess that we can do
       up to 5.1 */
    for (uint i = 2; i < 7; i++)
        settings->AddSupportedChannels(i);

    settings->SetPassthrough(PassthroughUnknown); // Maybe passthrough

    return settings;
}

bool AudioOutputDX::OpenDevice(void)
{
    WAVEFORMATEXTENSIBLE wf;
    DSBUFFERDESC         dsbdesc;

    CloseDevice();

    m_useSPDIF = m_passthrough || m_encode;
    m_priv->InitDirectSound(m_useSPDIF);
    if (!m_priv->m_directSound || !m_priv->m_directSoundLib)
    {
        LOG(VB_GENERAL, LOG_INFO, "DirectSound initialization failed");
        return false;
    }

    // fragments are 50ms worth of samples
    m_fragmentSize = 50 * m_outputBytesPerFrame * m_samplerate / 1000;
    // DirectSound buffer holds 4 fragments = 200ms worth of samples
    m_soundcardBufferSize = m_fragmentSize << 2;

    LOG(VB_AUDIO, LOG_INFO, QString("DirectSound buffer size: %1").arg(m_soundcardBufferSize));

    wf.Format.nChannels            = m_channels;
    wf.Format.nSamplesPerSec       = m_samplerate;
    wf.Format.nBlockAlign          = m_outputBytesPerFrame;
    wf.Format.nAvgBytesPerSec      = m_samplerate * m_outputBytesPerFrame;
    wf.Format.wBitsPerSample       = (m_outputBytesPerFrame << 3) / m_channels;
    wf.Samples.wValidBitsPerSample = AudioOutputSettings::FormatToBits(m_outputFormat);

    if (m_useSPDIF)
    {
        wf.Format.wFormatTag = WAVE_FORMAT_DOLBY_AC3_SPDIF;
        wf.SubFormat         = TORC2_KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF;
    }
    else if (m_outputFormat == FORMAT_FLT)
    {
        wf.Format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        wf.SubFormat         = TORC2_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    }
    else
    {
        wf.Format.wFormatTag = WAVE_FORMAT_PCM;
        wf.SubFormat         = TORC2_KSDATAFORMAT_SUBTYPE_PCM;
    }

    LOG(VB_AUDIO, LOG_INFO, QString("New format: %1bits %2ch %3Hz %4")
            .arg(wf.Samples.wValidBitsPerSample).arg(m_channels)
            .arg(m_samplerate).arg(m_useSPDIF ? "data" : "PCM"));

    if (m_channels <= 2)
        wf.Format.cbSize = 0;
    else
    {
        wf.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wf.dwChannelMask = 0x003F; // 0x003F = 5.1 channels
        wf.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    }

    memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));
    dsbdesc.dwSize = sizeof(DSBUFFERDESC);
    dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2  // Better position accuracy
                    | DSBCAPS_GLOBALFOCUS          // Allows background playing
                    | DSBCAPS_LOCHARDWARE;         // Needed for 5.1 on emu101k

    if (!m_useSPDIF)
        dsbdesc.dwFlags |= DSBCAPS_CTRLVOLUME;     // Allow volume control

    dsbdesc.dwBufferBytes = m_soundcardBufferSize; // buffer size
    dsbdesc.lpwfxFormat = (WAVEFORMATEX *)&wf;

    if (FAILED(IDirectSound_CreateSoundBuffer(m_priv->m_directSound, &dsbdesc,
                                            &m_priv->m_directSoundBuffer, NULL)))
    {
        /* Vista does not support hardware mixing
           try without DSBCAPS_LOCHARDWARE */
        dsbdesc.dwFlags &= ~DSBCAPS_LOCHARDWARE;
        HRESULT dsresult = IDirectSound_CreateSoundBuffer(m_priv->m_directSound, &dsbdesc, &m_priv->m_directSoundBuffer, NULL);
        if (FAILED(dsresult))
        {
            if (dsresult == DSERR_UNSUPPORTED)
                LOG(VB_GENERAL, LOG_INFO, QString("Unsupported format for device %1:%2")
                      .arg(m_priv->m_deviceNum).arg(m_priv->m_deviceName));
            else
                LOG(VB_GENERAL, LOG_INFO, QString("Failed to create DS buffer 0x%1")
                      .arg((DWORD)dsresult, 0, 16));
            return false;
        }

        LOG(VB_AUDIO, LOG_INFO, "Using software mixer");
    }

    LOG(VB_AUDIO, LOG_INFO, "Created DirectSound buffer");
    return true;
}

void AudioOutputDX::CloseDevice(void)
{
    if (m_priv->m_directSoundBuffer)
        m_priv->DestroyDSBuffer();
}

void AudioOutputDX::WriteAudio(unsigned char * buffer, int size)
{
    if (size == 0)
        return;

    m_priv->FillBuffer(buffer, size);
    if (!m_priv->m_playStarted)
        m_priv->StartPlayback();
}

int AudioOutputDX::GetBufferedOnSoundcard(void) const
{
    if (!m_priv->m_playStarted)
        return 0;

    HRESULT dsresult;
    DWORD   playpos;
    DWORD   writepos;
    int     buffered;

    dsresult = IDirectSoundBuffer_GetCurrentPosition(m_priv->m_directSoundBuffer, &playpos, &writepos);
    if (dsresult != DS_OK)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot get current buffer position");
        return 0;
    }

    buffered = (int)m_priv->m_bufferPosition - (int)playpos;

    if (buffered <= 0)
        buffered += m_soundcardBufferSize;

    return buffered;
}

int AudioOutputDX::GetVolumeChannel(int Channel) const
{
    (void) Channel;

    HRESULT dsresult;
    long dxVolume = 0;
    int volume;

    if (m_useSPDIF)
        return 100;

    dsresult = IDirectSoundBuffer_GetVolume(m_priv->m_directSoundBuffer, &dxVolume);
    volume = (int)(pow(10, (float)dxVolume / 20.0) * 100.0);

    if (dsresult != DS_OK)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to get volume %1").arg(dxVolume));
        return volume;
    }

    LOG(VB_AUDIO, LOG_INFO, QString("Got volume %1").arg(volume));
    return volume;
}

void AudioOutputDX::SetVolumeChannel(int Channel, int Volume)
{
    (void)Channel;

    HRESULT dsresult;
    float dbAtten = 20 * log10((float)Volume / 100.0);
    long dxVolume = (Volume == 0) ? DSBVOLUME_MIN : (long)(100.0f * dbAtten);

    if (m_useSPDIF)
        return;

    // dxVolume is attenuation in 100ths of a decibel
    dsresult = IDirectSoundBuffer_SetVolume(m_priv->m_directSoundBuffer, dxVolume);

    if (dsresult != DS_OK)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to set volume %1").arg(dxVolume));
        return;
    }

    LOG(VB_AUDIO, LOG_INFO, QString("Set volume %1").arg(dxVolume));
}

class AudioFactoryDX : public AudioFactory
{
    void Score(const AudioSettings &Settings, AudioWrapper *Parent, int &Score)
    {
        (void)Parent;
        bool match = Settings.m_mainDevice.startsWith("directx", Qt::CaseInsensitive);
        int score  =  match ? AUDIO_PRIORITY_MATCH : AUDIO_PRIORITY_MEDIUM;
        if (Score <= score)
            Score = score;
    }

    AudioOutput* Create(const AudioSettings &Settings, AudioWrapper *Parent, int &Score)
    {
        bool match = Settings.m_mainDevice.startsWith("directx", Qt::CaseInsensitive);
        int score  =  match ? AUDIO_PRIORITY_MATCH : AUDIO_PRIORITY_MEDIUM;
        if (Score <= score)
            return new AudioOutputDX(Settings, Parent);
        return NULL;
    }

    void GetDevices(QList<AudioDeviceConfig> &DeviceList)
    {
        AudioOutputDXPrivate *priv = new AudioOutputDXPrivate(NULL);
        priv->InitDirectSound(false);
        QMap<int, QString> devices(tmp_priv->m_deviceList);
        delete tmp_priv;

        QMap<int, QString>::iterator it = devices.begin();
        for ( ; it != devices.end(); ++it)
        {
            AudioDeviceConfig *config = AudioOutput::GetAudioDeviceConfig(QString("DirectX:%1").arg(it.value()), QString());
            if (config)
            {
                DeviceList.append(*config);
                delete config;
            }
        }
    }
} AudioFactoryDX;

