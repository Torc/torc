/* -*- Mode: c++ -*-
 *
 * Copyright (C) 2010 foobum@gmail.com and jyavenard@gmail.com
 *
 * Licensed under the GPL v2 or a later version at your choosing.
 */

// Torc
#include "torclocalcontext.h"
#include "torclogging.h"
#include "torcavutils.h"
#include "audiooutputsettings.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

static const int srs[] = { 5512, 8000,  11025, 16000, 22050, 32000,  44100,
                           48000, 88200, 96000, 176400, 192000 };

static const AudioFormat fmts[] = { FORMAT_U8,  FORMAT_S16, FORMAT_S24LSB,
                                    FORMAT_S24, FORMAT_S32, FORMAT_FLT };

AudioOutputSettings::AudioOutputSettings(bool Invalid)
  : m_passthrough(PassthroughNo),
    m_features(FEATURE_NONE),
    m_invalid(Invalid),
    m_hasELD(false),
    m_ELD(AudioELD())
{
    for (int i = 0; i < 12; ++i)
        m_sr.append(srs[i]);
    for (int i = 0; i < 6; ++i)
        m_sf.append(fmts[i]);
}

AudioOutputSettings::~AudioOutputSettings()
{
    m_sr.clear();
    m_rates.clear();
    m_sf.clear();
    m_formats.clear();
    m_channels.clear();
}

AudioOutputSettings& AudioOutputSettings::operator=(const AudioOutputSettings &rhs)
{
    if (this == &rhs)
        return *this;
    m_sr            = rhs.m_sr;
    m_rates         = rhs.m_rates;
    m_sf            = rhs.m_sf;
    m_formats       = rhs.m_formats;
    m_channels      = rhs.m_channels;
    m_passthrough   = rhs.m_passthrough;
    m_features      = rhs.m_features;
    m_invalid       = rhs.m_invalid;
    m_hasELD        = rhs.m_hasELD;
    m_ELD           = rhs.m_ELD;
    return *this;
}

QList<int> AudioOutputSettings::GetRates(void)
{
    return m_sr;
}

void AudioOutputSettings::AddSupportedRate(int Rate)
{
    m_rates.append(Rate);
    LOG(VB_AUDIO, LOG_INFO, QString("Sample rate %1 is supported").arg(Rate));
}

bool AudioOutputSettings::IsSupportedRate(int Rate)
{
    if (m_rates.empty() && Rate == 48000)
        return true;

    QList<int>::iterator it = m_rates.begin();
    for ( ; it != m_rates.end(); ++it)
        if (*it == Rate)
            return true;

    return false;
}

int AudioOutputSettings::BestSupportedRate(void)
{
    if (m_rates.empty())
        return 48000;

    return m_rates.back();
}

int AudioOutputSettings::NearestSupportedRate(int Rate)
{
    if (m_rates.empty())
        return 48000;

    QList<int>::iterator it = m_rates.begin();

    // Assume rates vector is sorted
    for ( ; it != m_rates.end(); ++it)
        if (*it >= Rate)
            return *it;

    // Not found, so return highest available rate
    return m_rates.back();
}

QList<AudioFormat> AudioOutputSettings::GetFormats(void)
{
    return m_sf;
}

void AudioOutputSettings::AddSupportedFormat(AudioFormat Format)
{
    LOG(VB_AUDIO, LOG_INFO, QString("Format %1 is supported").arg(FormatToString(Format)));
    m_formats.append(Format);
}

bool AudioOutputSettings::IsSupportedFormat(AudioFormat Format)
{
    if (m_formats.empty() && Format == FORMAT_S16)
        return true;

    QList<AudioFormat>::iterator it = m_formats.begin();
    for ( ; it != m_formats.end(); ++it)
        if (*it == Format)
            return true;

    return false;
}

AudioFormat AudioOutputSettings::BestSupportedFormat(void)
{
    if (m_formats.empty())
        return FORMAT_S16;
    return m_formats.back();
}

int AudioOutputSettings::FormatToBits(AudioFormat Format)
{
    switch (Format)
    {
        case FORMAT_U8:     return 8;
        case FORMAT_S16:    return 16;
        case FORMAT_S24LSB:
        case FORMAT_S24:    return 24;
        case FORMAT_S32:
        case FORMAT_FLT:    return 32;
        default:            break;
    }

    return -1;
}

const char* AudioOutputSettings::FormatToString(AudioFormat Format)
{
    switch (Format)
    {
        case FORMAT_U8:     return "unsigned 8 bit";
        case FORMAT_S16:    return "signed 16 bit";
        case FORMAT_S24:    return "signed 24 bit MSB";
        case FORMAT_S24LSB: return "signed 24 bit LSB";
        case FORMAT_S32:    return "signed 32 bit";
        case FORMAT_FLT:    return "32 bit floating point";
        default:            return "unknown";
    }
}

int AudioOutputSettings::SampleSize(AudioFormat Format)
{
    switch (Format)
    {
        case FORMAT_U8:     return 1;
        case FORMAT_S16:    return 2;
        case FORMAT_S24:
        case FORMAT_S24LSB:
        case FORMAT_S32:
        case FORMAT_FLT:    return 4;
        default:            break;
    }

    return 0;
}

void AudioOutputSettings::AddSupportedChannels(int Channels)
{
    m_channels.append(Channels);
    LOG(VB_AUDIO, LOG_INFO, QString("%1 channel(s) are supported").arg(Channels));
}

bool AudioOutputSettings::IsSupportedChannels(int Channels)
{
    if (m_channels.empty() && Channels == 2)
        return true;

    QList<int>::iterator it = m_channels.begin();
    for ( ; it != m_channels.end(); ++it)
        if (*it == Channels)
            return true;

    return false;
}

int AudioOutputSettings::BestSupportedChannels(void)
{
    if (m_channels.empty())
        return 2;

    return m_channels.back();
}

void AudioOutputSettings::SetPassthrough(Passthrough NewPassthrough)
{
    m_passthrough = NewPassthrough;
}

Passthrough AudioOutputSettings::CanPassthrough(void)
{
    return m_passthrough;
}

bool AudioOutputSettings::CanFeature(DigitalFeature Feature)
{
    return m_features & Feature;
}

bool AudioOutputSettings::CanFeature(unsigned int Feature)
{
    return m_features & Feature;
}

bool AudioOutputSettings::CanAC3(void)
{
    return m_features & FEATURE_AC3;
}

bool AudioOutputSettings::CanDTS(void)
{
    return m_features & FEATURE_DTS;
}

bool AudioOutputSettings::CanLPCM(void)
{
    return m_features & FEATURE_LPCM;
}

bool AudioOutputSettings::IsInvalid(void)
{
    return m_invalid;
}

void AudioOutputSettings::SetFeature(DigitalFeature Feature)
{
    m_features |= Feature;
}

void AudioOutputSettings::SetFeature(unsigned int Feature)
{
    m_features |= Feature;
}

void AudioOutputSettings::SetBestSupportedChannels(int Channels)
{
    if (m_channels.empty())
    {
        m_channels.append(Channels);
        return;
    }

    while (!m_channels.isEmpty() && m_channels.last() >= Channels)
        m_channels.removeLast();

    m_channels.append(Channels);
}

void AudioOutputSettings::SetFeature(bool Set, int Feature)
{
    if (Set)
        m_features |= Feature;
    else
        m_features &= ~Feature;
}

void AudioOutputSettings::SetFeature(bool Set, DigitalFeature Feature)
{
    SetFeature(Set, (int)Feature);
}

/**
 * Returns capabilities supported by the audio device
 * amended to take into account the digital audio
 * options (AC3 and DTS)
 * Warning: do not call it twice in a row, will lead to invalid settings
 */
AudioOutputSettings* AudioOutputSettings::GetCleaned(bool NewCopy)
{
    AudioOutputSettings* aosettings;

    if (NewCopy)
    {
        aosettings = new AudioOutputSettings;
        *aosettings = *this;
    }
    else
    {
        aosettings = this;
    }

    if (m_invalid)
        return aosettings;

    if (BestSupportedPCMChannelsELD() > 2)
    {
        aosettings->SetFeature(FEATURE_LPCM);
    }

    if (IsSupportedFormat(FORMAT_S16))
    {
        // E-AC3 is transferred as stereo PCM at 4 times the rates
        // assume all amplifier supporting E-AC3 also supports 7.1 LPCM
        // as it's mandatory under the bluray standard
        if (m_passthrough >= 0 && IsSupportedChannels(8) && IsSupportedRate(192000))
            aosettings->SetFeature(FEATURE_TRUEHD | FEATURE_DTSHD | FEATURE_EAC3);

        if (m_passthrough >= 0)
        {
            if (BestSupportedChannels() == 2)
            {
                LOG(VB_AUDIO, LOG_INFO, "may be AC3 or DTS capable");
                aosettings->AddSupportedChannels(6);
            }
            aosettings->SetFeature(FEATURE_AC3 | FEATURE_DTS);
        }
    }
    else
    {
        // Can't do digital passthrough without 16 bits audio
        aosettings->SetPassthrough(PassthroughNo);
        aosettings->SetFeature(false, FEATURE_AC3 | FEATURE_DTS | FEATURE_EAC3 | FEATURE_TRUEHD | FEATURE_DTSHD);
    }

    return aosettings;
}

/**
 * Returns capabilities supported by the audio device
 * amended to take into account the digital audio
 * options (AC3 and DTS) as well as the user settings
 * If NewCopy = false, assume GetCleaned was called before hand
 */
AudioOutputSettings* AudioOutputSettings::GetUsers(bool NewCopy)
{
    AudioOutputSettings* aosettings = NewCopy ? GetCleaned(NewCopy) : this;

    if (aosettings->m_invalid)
        return aosettings;

    int currentchannels = gLocalContext->GetSetting(TORC_AUDIO + "MaxChannels", 2);
    int maxchannels     = aosettings->BestSupportedChannels();

    bool bAC3  = aosettings->CanFeature(FEATURE_AC3) &&
        gLocalContext->GetSetting(TORC_AUDIO + "AC3PassThru", false);

    bool bDTS  = aosettings->CanFeature(FEATURE_DTS) &&
        gLocalContext->GetSetting(TORC_AUDIO + "DTSPassThru", false);

    bool bLPCM = aosettings->CanFeature(FEATURE_LPCM) &&
        !gLocalContext->GetSetting(TORC_AUDIO + "StereoPCM", false);

    bool bEAC3 = aosettings->CanFeature(FEATURE_EAC3) &&
        gLocalContext->GetSetting(TORC_AUDIO + "EAC3PassThru", false) &&
        !gLocalContext->GetSetting(TORC_AUDIO + "Audio48kOverride", false);

    // TrueHD requires HBR support.
    bool bTRUEHD = aosettings->CanFeature(FEATURE_TRUEHD) &&
        gLocalContext->GetSetting(TORC_AUDIO + "TrueHDPassThru", false) &&
        !gLocalContext->GetSetting(TORC_AUDIO + "Audio48kOverride", false) &&
        gLocalContext->GetSetting(TORC_AUDIO + "HBRPassthru", true);

    bool bDTSHD = aosettings->CanFeature(FEATURE_DTSHD) &&
        gLocalContext->GetSetting(TORC_AUDIO + "DTSHDPassThru", false) &&
        !gLocalContext->GetSetting(TORC_AUDIO + "Audio48kOverride", false);

    if (maxchannels > 2 && !bLPCM)
        maxchannels = 2;
    if (maxchannels == 2 && (bAC3 || bDTS))
        maxchannels = 6;

    if (currentchannels > maxchannels)
        currentchannels = maxchannels;

    aosettings->SetBestSupportedChannels(currentchannels);
    aosettings->SetFeature(bAC3, FEATURE_AC3);
    aosettings->SetFeature(bDTS, FEATURE_DTS);
    aosettings->SetFeature(bLPCM, FEATURE_LPCM);
    aosettings->SetFeature(bEAC3, FEATURE_EAC3);
    aosettings->SetFeature(bTRUEHD, FEATURE_TRUEHD);
    aosettings->SetFeature(bDTSHD, FEATURE_DTSHD);

    return aosettings;
}

int AudioOutputSettings::GetMaxHDRate()
{
    if (!CanFeature(FEATURE_DTSHD))
        return 0;

    // If no HBR or no LPCM, limit bitrate to 6.144Mbit/s
    if (!gLocalContext->GetSetting(TORC_AUDIO + "HBRPassthru", true) ||
        !CanFeature(FEATURE_LPCM))
    {
        return 192000;  // E-AC3/DTS-HD High Res: 192k, 16 bits, 2 ch
    }

    return 768000;      // TrueHD or DTS-HD MA: 192k, 16 bits, 8 ch
}

#define ARG(x) ((tmp.isEmpty() ? "" : ",") + QString(x))
    
QString AudioOutputSettings::FeaturesToString(DigitalFeature Feature)
{
    QString result;

    DigitalFeature feature[7] =
    {
        FEATURE_AC3,
        FEATURE_DTS,
        FEATURE_LPCM,
        FEATURE_EAC3,
        FEATURE_TRUEHD,
        FEATURE_DTSHD,
        FEATURE_AAC
    };

    static const QString features[7] =
    {
        "AC3",
        "DTS",
        "LPCM",
        "EAC3",
        "TRUEHD",
        "DTSHD",
        "AAC"
    };
    
    for (unsigned int i = 0; i < 7; i++)
        if (Feature & feature[i])
            result += features[i];

    return result;
}

QString AudioOutputSettings::FeaturesToString(void)
{
    return FeaturesToString((DigitalFeature)m_features);
}

QString AudioOutputSettings::GetPassthroughParams(int Codec, int CodecProfile,
                                                  int &Samplerate, int &Channels,
                                                  bool CanDTSHDMA)
{
    QString log;

    Channels = 2;

    switch (Codec)
    {
        case AV_CODEC_ID_AC3:
            log = "AC3";
            break;
        case AV_CODEC_ID_EAC3:
            Samplerate = Samplerate * 4;
            log = "Dolby Digital Plus (E-AC3)";
            break;
        case AV_CODEC_ID_DTS:
            switch(CodecProfile)
            {
                case FF_PROFILE_DTS_ES:
                    log = "DTS-ES";
                    break;
                case FF_PROFILE_DTS_96_24:
                    log = "DTS 96/24";
                    break;
                case FF_PROFILE_DTS_HD_HRA:
                    Samplerate = 192000;
                    log = "DTS-HD High-Res";
                    break;
                case FF_PROFILE_DTS_HD_MA:
                    Samplerate = 192000;
                    if (CanDTSHDMA)
                    {
                        log = "DTS-HD MA";
                        Channels = 8;
                    }
                    else
                    {
                        log = "DTS-HD High-Res";
                    }
                    break;
                case FF_PROFILE_DTS:
                default:
                    log = "DTS Core";
                    break;
            }
            break;
        case AV_CODEC_ID_TRUEHD:
            Channels = 8;
            log = "TrueHD";
            switch(Samplerate)
            {
                case 48000:
                case 96000:
                case 192000:
                    Samplerate = 192000;
                    break;
                case 44100:
                case 88200:
                case 176400:
                    Samplerate = 176400;
                    break;
                default:
                    log = "TrueHD: Unsupported samplerate";
                    break;
            }
            break;
        default:
            break;
    }
    return log;
}

bool AudioOutputSettings::HasELD()
{
    return m_hasELD && m_ELD.IsValid();
}

AudioELD& AudioOutputSettings::GetELD(void)
{
    return m_ELD;
}

void AudioOutputSettings::SetELD(const QByteArray &Data)
{
    m_hasELD = true;
    m_ELD    = AudioELD(Data);
}

int AudioOutputSettings::BestSupportedChannelsELD(void)
{
    int chan = AudioOutputSettings::BestSupportedChannels();
    if (!HasELD())
        return chan;

    int eld = m_ELD.GetMaxChannels();
    return eld < chan ? eld : chan;
}

int AudioOutputSettings::BestSupportedPCMChannelsELD(void)
{
    int chan = AudioOutputSettings::BestSupportedChannels();
    if (!HasELD())
        return chan;

    int eld = m_ELD.GetMaxLPCMChannels();
    return eld < chan ? eld : chan;
}

AudioDescription::AudioDescription()
  : m_codecId(AV_CODEC_ID_NONE),
    m_format(FORMAT_NONE),
    m_sampleSize(-1),
    m_sampleRate(-1),
    m_channels(-1),
    m_codecProfile(0),
    m_passthrough(false),
    m_originalChannels(-1),
    m_bufferTime(1),
    m_bestFillSize(1)
{
}

AudioDescription::AudioDescription(int  Codec,       AudioFormat Format,
                                   int  Samplerate,  int Channels,
                                   bool Passthrough, int OriginalChannels,
                                   int  Profile)
  : m_codecId(Codec),
    m_format(Format),
    m_sampleSize(Channels * AudioOutputSettings::SampleSize(Format)),
    m_sampleRate(Samplerate),
    m_channels(Channels),
    m_codecProfile(Profile),
    m_passthrough(Passthrough),
    m_originalChannels(OriginalChannels),
    m_bufferTime(100) //ms
{
    m_bestFillSize = (m_sampleSize * m_sampleRate) * m_bufferTime / 1000;
}

QString AudioDescription::ToString(void)
{
    return QString("'%1' %2Hz %3ch %4bps %5(profile %6)")
        .arg(AVCodecToString((AVCodecID)m_codecId))
        .arg(m_sampleRate)
        .arg(m_channels)
        .arg(AudioOutputSettings::FormatToBits(m_format))
        .arg(m_passthrough ? "Passthrough " : "")
        .arg(m_codecProfile);
}
