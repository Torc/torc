/* AudioELD
*
* This file is part of the Torc project.

* Torc modifications
* Copyright (C) Mark Kendall 2012
*
* Adapted from ELD class which is part of the MythTV project,
* Copyright (C) Jean-Yves Avenard <jyavenard@mythtv.org>
*
* which was based on ALSA hda_eld.c
* Copyright (C) 2008 Intel Corporation.
*
* Authors:
*         Wu Fengguang <wfg@linux.intel.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/
// Qt
#include <QtEndian>

// Torc
#include "torclogging.h"
#include "audioeld.h"

#define ELD_FIXED_SIZE     20
#define ELD_MAX_SIZE       256
#define ELD_MAX_NAMELENGTH 16

/*! \class AudioELD
 *  \brief A class to parse ELD data.
 */

static const QString audiotype_names[18] = {
    /*  0 */ "Undefined",
    /*  1 */ "LPCM",
    /*  2 */ "AC3",
    /*  3 */ "MPEG1",
    /*  4 */ "MP3",
    /*  5 */ "MPEG2",
    /*  6 */ "AAC-LC",
    /*  7 */ "DTS",
    /*  8 */ "ATRAC",
    /*  9 */ "DSD (One Bit Audio)",
    /* 10 */ "E-AC3",
    /* 11 */ "DTS-HD",
    /* 12 */ "TrueHD",
    /* 13 */ "DST",
    /* 14 */ "WMAPro",
    /* 15 */ "HE-AAC",
    /* 16 */ "HE-AACv2",
    /* 17 */ "MPEG Surround",
};

#define SNDRV_PCM_RATE_5512   (1<<0)
#define SNDRV_PCM_RATE_8000   (1<<1)
#define SNDRV_PCM_RATE_11025  (1<<2)
#define SNDRV_PCM_RATE_16000  (1<<3)
#define SNDRV_PCM_RATE_22050  (1<<4)
#define SNDRV_PCM_RATE_32000  (1<<5)
#define SNDRV_PCM_RATE_44100  (1<<6)
#define SNDRV_PCM_RATE_48000  (1<<7)
#define SNDRV_PCM_RATE_64000  (1<<8)
#define SNDRV_PCM_RATE_88200  (1<<9)
#define SNDRV_PCM_RATE_96000  (1<<10)
#define SNDRV_PCM_RATE_176400 (1<<11)
#define SNDRV_PCM_RATE_192000 (1<<12)

static int CEASamplingFrequencies[8] =
{
    0, // Refer to Stream Header
    SNDRV_PCM_RATE_32000,
    SNDRV_PCM_RATE_44100,
    SNDRV_PCM_RATE_48000,
    SNDRV_PCM_RATE_88200,
    SNDRV_PCM_RATE_96000,
    SNDRV_PCM_RATE_176400,
    SNDRV_PCM_RATE_192000
};

#define GRAB_BITS(buf, byte, lowbit, bits) ((buf[byte] >> (lowbit)) & ((1 << (bits)) - 1))

AudioELD::CEASad::CEASad()
  : m_channels(0),
    m_format(0),
    m_rates(0),
    m_sampleBits(0),
    m_maxBitrate(0),
    m_profile(0)
{
}

AudioELD::AudioELD(const QByteArray &ELD)
  : m_eld(ELD),
    m_valid(false),
    m_eldVersion(0),
    m_edidVersion(0),
    m_manufacturerId(0),
    m_productId(0),
    m_portId(0),
    m_formats(0),
    m_supportsHDCP(0),
    m_supportsAI(0),
    m_connectionType(0),
    m_audioSyncDelay(0),
    m_speakerAllocation(0),
    m_numSADs(0)
{
    Parse();
}

AudioELD::AudioELD()
  : m_eld(QByteArray()),
    m_valid(false),
    m_eldVersion(0),
    m_edidVersion(0),
    m_manufacturerId(0),
    m_productId(0),
    m_portId(0),
    m_formats(0),
    m_supportsHDCP(0),
    m_supportsAI(0),
    m_connectionType(0),
    m_audioSyncDelay(0),
    m_speakerAllocation(0),
    m_numSADs(0)
{
}

AudioELD::~AudioELD()
{
}

QString AudioELD::GetELDVersionName(ELDVersions Version)
{
    switch (Version)
    {
        case ELD_VER_CEA_861D: return QString("CEA-861D or below");
        case ELD_VER_PARTIAL:  return QString("Partial");
        default:
            break;
    }

    return QString("Unknown");
}

bool AudioELD::IsValid(void)
{
    return m_valid;
}

void AudioELD::Debug(void)
{
    if (!IsValid())
    {
        LOG(VB_AUDIO, LOG_INFO, "Invalid ELD");
        return;
    }

    static const QString connectionnames[4] = { "HDMI", "DisplayPort", "Reserved 1", "Reserved 2" };
    m_connectionName = connectionnames[m_connectionType];

    LOG(VB_AUDIO, LOG_INFO, QString("Detected monitor '%1', connection type '%2'")
        .arg(m_monitorName).arg(m_connectionName));
    LOG(VB_AUDIO, LOG_INFO, QString("ELD version: ").arg(GetELDVersionName((ELDVersions)m_eldVersion)));

    if (m_speakerAllocation)
    {
        static const QString speakernames[11] = { "FL/FR", "LFE", "FC", "RL/RR", "RC", "FLC/FRC", "RLC/RRC", "FLW/FRW", "FLH/FRH", "TC", "FCH" };

        QString names;

        for (int i = 0; i < 11; i++)
            if ((m_speakerAllocation & (1 << i)) != 0)
                names += QString(" %1").arg(speakernames[i]);

        LOG(VB_AUDIO, LOG_INFO, QString("Available speakers:%1").arg(names));
    }

    LOG(VB_AUDIO, LOG_INFO, QString("Max LPCM channels: %1").arg(GetMaxLPCMChannels()));
    LOG(VB_AUDIO, LOG_INFO, QString("Max channels: %1").arg(GetMaxChannels()));
    LOG(VB_AUDIO, LOG_INFO, QString("Supported codecs: %1").arg(GetCodecsDescription()));

    for (int i = 0; i < m_numSADs; i++)
        LOG(VB_AUDIO, LOG_INFO, DebugSAD(i));

    LOG(VB_AUDIO, LOG_DEBUG, QString("manufacturerId : 0x%1").arg(m_manufacturerId, 0, 16));
    LOG(VB_AUDIO, LOG_DEBUG, QString("productId      : 0x%1").arg(m_productId, 0, 16));
    LOG(VB_AUDIO, LOG_DEBUG, QString("portId         : 0x%1").arg(m_portId, 0, 16));
    LOG(VB_AUDIO, LOG_DEBUG, QString("supportsHDCP   : %1").arg(m_supportsHDCP));
    LOG(VB_AUDIO, LOG_DEBUG, QString("supportsAI     : %1").arg(m_supportsAI));
    LOG(VB_AUDIO, LOG_DEBUG, QString("audioSyncDelay : %1").arg(m_audioSyncDelay));
    LOG(VB_AUDIO, LOG_DEBUG, QString("Num SADs       : %1").arg(m_numSADs));
}

int AudioELD::GetMaxLPCMChannels(void)
{
    int channels = 2; // assume stereo at the minimum

    for (int i = 0; i < m_numSADs; i++)
        if (m_SADs[i].m_format == TYPE_LPCM)
            if (m_SADs[i].m_channels > channels)
                channels = m_SADs[i].m_channels;

    return channels;
}

int AudioELD::GetMaxChannels(void)
{
    int channels = 2; // assume stereo at the minimum

    for (int i = 0; i < m_numSADs; i++)
        if (m_SADs[i].m_channels > channels)
            channels = m_SADs[i].m_channels;

    return channels;
}

QString AudioELD::GetProductName(void)
{
    return m_monitorName;
}

QString AudioELD::GetConnectionName(void)
{
    return m_connectionName;
}

QString AudioELD::GetCodecsDescription(void)
{
    QString codecs;
    bool first = true;
    for (int i = 0; i < 18; i++)
    {
        if ((m_formats & (1 << i)) != 0)
        {
            if (!first)
                codecs += ", ";
            codecs += audiotype_names[i];
            first = false;
        }
    }

    return codecs;
}

void AudioELD::Parse(void)
{
    int size = m_eld.size();
    if (size < 1)
        return;

    const char* buffer  = m_eld.constData();
    m_eldVersion        = GRAB_BITS(buffer, 0, 3, 5);
    int namelength      = GRAB_BITS(buffer, 4, 0, 5);
    m_edidVersion       = GRAB_BITS(buffer, 4, 5, 3);
    m_supportsHDCP      = GRAB_BITS(buffer, 5, 0, 1);
    m_supportsAI        = GRAB_BITS(buffer, 5, 1, 1);
    m_connectionType    = GRAB_BITS(buffer, 5, 2, 2);
    m_numSADs           = GRAB_BITS(buffer, 5, 4, 4);
    m_audioSyncDelay    = GRAB_BITS(buffer, 6, 0, 8) * 2;
    m_speakerAllocation = GRAB_BITS(buffer, 7, 0, 7);
    m_portId            = qFromLittleEndian(*((uint64_t *)(buffer + 8)));
    m_manufacturerId    = qFromLittleEndian(*((uint16_t *)(buffer + 16)));
    m_productId         = qFromLittleEndian(*((uint16_t *)(buffer + 18)));

    if (m_eldVersion != ELD_VER_CEA_861D && m_eldVersion != ELD_VER_PARTIAL)
    {
        LOG(VB_AUDIO, LOG_ERR, QString("Unknown ELD version %1").arg(m_eldVersion));
        return;
    }

    if ((namelength > ELD_MAX_NAMELENGTH) || (ELD_FIXED_SIZE + namelength > size))
    {
        LOG(VB_AUDIO, LOG_ERR, QString("Range error (name length %1)").arg(namelength));
        return;
    }

    if (m_numSADs > ELD_MAX_NUM_SADS)
    {
        LOG(VB_AUDIO, LOG_ERR, "Number of SADs out of range");
        return;
    }

    m_monitorName = QString::fromAscii((char*)buffer + ELD_FIXED_SIZE, namelength + 1).simplified();

    for (int i = 0; i < m_numSADs; i++)
    {
        if (ELD_FIXED_SIZE + namelength + 3 * (i + 1) > size)
        {
            LOG(VB_AUDIO, LOG_ERR, QString("out of range SAD %1").arg(i));
            return;
        }

        ParseSAD(i, buffer + ELD_FIXED_SIZE + namelength + 3 * i);
    }

    if (!m_speakerAllocation)
        m_speakerAllocation = 0xffff;

    m_valid = true;

    Debug();
}

void AudioELD::ParseSAD(int Index, const char *Buffer)
{
    int val = GRAB_BITS(Buffer, 1, 0, 7);
    m_SADs[Index].m_rates = 0;
    for (int i = 0; i < 7; i++)
        if ((val & (1 << i)) != 0)
            m_SADs[Index].m_rates |= CEASamplingFrequencies[i + 1];

    m_SADs[Index].m_channels = GRAB_BITS(Buffer, 0, 0, 3) + 1;

    m_SADs[Index].m_sampleBits = 0;
    m_SADs[Index].m_maxBitrate = 0;

    m_SADs[Index].m_format = GRAB_BITS(Buffer, 0, 3, 4);
    m_formats |= 1 << m_SADs[Index].m_format;
    switch (m_SADs[Index].m_format)
    {
        case TYPE_REF_STREAM_HEADER:
            LOG(VB_AUDIO, LOG_ERR, "Audio coding type '0' not expected");
            break;
        case TYPE_LPCM:
            m_SADs[Index].m_sampleBits = GRAB_BITS(Buffer, 2, 0, 3);
            break;
        case TYPE_AC3:
        case TYPE_MPEG1:
        case TYPE_MP3:
        case TYPE_MPEG2:
        case TYPE_AACLC:
        case TYPE_DTS:
        case TYPE_ATRAC:
            m_SADs[Index].m_maxBitrate = GRAB_BITS(Buffer, 2, 0, 8);
            m_SADs[Index].m_maxBitrate *= 8000;
            break;
        case TYPE_SACD:
        case TYPE_EAC3:
        case TYPE_DTS_HD:
        case TYPE_MLP:
        case TYPE_DST:
            break;
        case TYPE_WMAPRO:
            m_SADs[Index].m_profile = GRAB_BITS(Buffer, 2, 0, 3);
            break;
        case TYPE_REF_CXT:
            m_SADs[Index].m_format = GRAB_BITS(Buffer, 2, 3, 5);
            if (m_SADs[Index].m_format == XTYPE_HE_REF_CT ||
                m_SADs[Index].m_format >= XTYPE_FIRST_RESERVED)
            {
                LOG(VB_AUDIO, LOG_INFO, QString("Audio coding xtype '%1'' not expected").arg(m_SADs[Index].m_format));
                m_SADs[Index].m_format = 0;
            }
            else
            {
                m_SADs[Index].m_format += TYPE_HE_AAC - XTYPE_HE_AAC;
            }
            break;
    }
}

QString AudioELD::DebugSAD(int Index)
{
    if (!m_SADs[Index].m_format)
        return QString("");

    static unsigned int pcmrates[12] = { 5512, 8000, 11025, 16000, 22050, 32000, 44100, 48000, 88200, 96000, 176400, 192000 };
    QString rates = QString();

    for (int i = 0; i < 12; i++)
        if ((m_SADs[Index].m_rates & (1 << i)) != 0)
            rates += QString(" %1").arg(pcmrates[i]);

    QString bits;

    if (m_SADs[Index].m_format == TYPE_LPCM)
    {
        bits = QString(", bits =");
        static unsigned int pcmbits[3] = { 16, 20, 24 };
        for (int i = 0; i < 3; i++)
            if ((m_SADs[Index].m_sampleBits & (1 << i)) != 0)
                bits += QString(" %1").arg(pcmbits[i]);
    }
    else if (m_SADs[Index].m_maxBitrate)
    {
        bits = QString(", max bitrate = %1").arg(m_SADs[Index].m_maxBitrate);
    }

    return QString("Supports coding type %1: channels = %2, rates =%3%4")
        .arg(audiotype_names[m_SADs[Index].m_format])
        .arg(m_SADs[Index].m_channels).arg(rates).arg(bits);
}
