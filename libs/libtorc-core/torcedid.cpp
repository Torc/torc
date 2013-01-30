/* Class TorcEDID
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012
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
#include <QMutex>

// Torc
#include "torclogging.h"
#include "torcedid.h"

TorcEDID* TorcEDID::gTorcEDID   = new TorcEDID();
QMutex* TorcEDID::gTorcEDIDLock = new QMutex(QMutex::Recursive);

/*! \class TorcEDID
 *  \brief A parser for Extended Display Identification Data (EDID)
 *
 * EDID provides details about the capability of a connected monitor or TV. Torc
 * uses this directly to assist with CEC setup and detection of 3D video modes.
 * The underlying video and audio libraries will already be using this data
 * to detect supported video and audio (over HDMI) modes.
*/

TorcEDID::TorcEDID()
  : m_physicalAddress(0x0000),
    m_audioLatency(0),
    m_videoLatency(0),
    m_interlacedAudioLatency(-1),
    m_interlacedVideoLatency(-1)
{
}

TorcEDID::~TorcEDID()
{
}

void TorcEDID::RegisterEDID(QByteArray Data)
{
    QMutexLocker locker(gTorcEDIDLock);

    if (!Data.isEmpty())
    {
        gTorcEDID->m_edidData = Data;
        gTorcEDID->Process();
    }
}

qint16 TorcEDID::PhysicalAddress(void)
{
    QMutexLocker locker(gTorcEDIDLock);

    return gTorcEDID->m_physicalAddress;
}

int TorcEDID::GetAudioLatency(bool Interlaced)
{
    QMutexLocker locker(gTorcEDIDLock);

    if (Interlaced && gTorcEDID->m_interlacedAudioLatency >=0 )
        return gTorcEDID->m_interlacedAudioLatency;

    return gTorcEDID->m_audioLatency;
}

int TorcEDID::GetVideoLatency(bool Interlaced)
{
    QMutexLocker locker(gTorcEDIDLock);

    if (Interlaced && gTorcEDID->m_interlacedVideoLatency >= 0)
        return gTorcEDID->m_interlacedVideoLatency;

    return gTorcEDID->m_videoLatency;;
}

void TorcEDID::Process(void)
{
    if (m_edidData.isEmpty())
        return;

    // EDID data should be in 128 byte blocks
    if ((m_edidData.size() < 128) || (m_edidData.size() & 0x7f))
    {
        LOG(VB_GENERAL, LOG_ERR, "EDID data size is invalid");
        return;
    }

    // checksum first 128 bytes
    qint8 sum = 0;
    for (int i = 0; i < 128; ++i)
        sum += m_edidData.at(i);

    if (sum != 0)
    {
        LOG(VB_GENERAL, LOG_ERR, "EDID checksum error");
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Processing EDID (%1 bytes)").arg(m_edidData.size()));

    // reset
    m_physicalAddress        = 0x0000;
    m_audioLatency           = 0;
    m_videoLatency           = 0;
    m_interlacedAudioLatency = -1;
    m_interlacedVideoLatency = -1;

    // extensions
    int size = m_edidData.size();
    if (size > 128)
    {
        for (int i = 1; i < size - 4; ++i)
        {
            if (m_edidData.at(i)     == 0x03 &&
                m_edidData.at(i + 1) == 0x0C &&
                m_edidData.at(i + 2) == 0x0)
            {
                int length = m_edidData.at(i - 1) & 0xf;
                LOG(VB_GUI, LOG_INFO, QString("HDMI VSDB size: %1").arg(length));

                if (length >= 5)
                {
                    m_physicalAddress = (m_edidData.at(i + 3) << 8) + m_edidData.at(i + 4);
                    LOG(VB_GENERAL, LOG_INFO, QString("EDID physical address: 0x%1").arg(m_physicalAddress,0,16));

                    if (length >= 8 && ((i + 7) < size))
                    {
                        int flags = (uint8_t)m_edidData.at(i + 7);
                        bool latencies  = flags & 0x80;
                        bool ilatencies = flags & 0x40;

                        if (latencies && length >= 10 && ((i + 9) < size))
                        {
                            uint8_t video = m_edidData.at(i + 8);
                            if (video > 0 && video <= 251)
                                m_videoLatency = (video - 1) * 2;

                            uint8_t audio = m_edidData.at(i + 9);
                            if (audio > 0 && audio <= 251)
                                m_audioLatency = (audio - 1) * 2;

                            LOG(VB_GUI, LOG_INFO, QString("HDMI latencies, audio: %1 video: %2").arg(m_audioLatency).arg(m_videoLatency));

                            if (ilatencies && length >= 12 && ((i + 11) < size))
                            {
                                uint8_t ivideo = m_edidData.at(i + 10);
                                if (ivideo > 0 && ivideo <= 251)
                                    m_interlacedVideoLatency = (ivideo - 1) * 2;

                                uint8_t iaudio = m_edidData.at(i + 11);
                                if (iaudio > 0 && iaudio <= 251)
                                    m_interlacedAudioLatency = (iaudio - 1) * 2;

                                LOG(VB_GUI, LOG_INFO, QString("HDMI interlaced latencies, audio: %1 video: %2")
                                    .arg(m_interlacedAudioLatency).arg(m_interlacedVideoLatency));
                            }
                        }
                    }
                }

                break;
            }
        }
    }

    // debug
    if (VERBOSE_LEVEL_CHECK(VB_GENERAL, LOG_DEBUG))
    {
        QString output("\n\n");
        for (int i = 0; i < m_edidData.size(); ++i)
        {
            output += QString("%1").arg(m_edidData.at(i) & 0xff, 2, 16, QLatin1Char('0'));
            if (!((i + 1) % 32) && i)
                output += "\n";
        }

        LOG(VB_GENERAL, LOG_DEBUG, output + "\n");
    }
}
