/* Class TorcEDID
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012-13
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
#include <QtEndian>

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

TorcEDID::TorcEDID(const QByteArray &Data)
  : m_edidData(Data),
    m_physicalAddress(0x0000),
    m_audioLatency(0),
    m_videoLatency(0),
    m_interlacedAudioLatency(-1),
    m_interlacedVideoLatency(-1),
    m_serialNumber(0)
{
    Process(true);
}

TorcEDID::TorcEDID()
  : m_physicalAddress(0x0000),
    m_audioLatency(0),
    m_videoLatency(0),
    m_interlacedAudioLatency(-1),
    m_interlacedVideoLatency(-1),
    m_serialNumber(0)
{
}

TorcEDID::~TorcEDID()
{
}

void TorcEDID::RegisterEDID(WId Window, int Screen)
{
    QMutexLocker locker(gTorcEDIDLock);

    // source edid
    QMap<QPair<int,QString>,QByteArray> edids;

    EDIDFactory* factory = EDIDFactory::GetEDIDFactory();
    for ( ; factory; factory = factory->NextFactory())
        factory->GetEDID(edids, Window, Screen);

    if (edids.isEmpty())
    {
        LOG(VB_GENERAL, LOG_INFO, "Failed to find EDID for monitor");
        return;
    }

    // pick the best
    int score = -1;
    QMap<QPair<int,QString>,QByteArray>::iterator best = edids.end();
    QMap<QPair<int,QString>,QByteArray>::iterator it = edids.begin();
    for ( ; it != edids.end(); ++it)
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("EDID from '%1': score %2").arg(it.key().second).arg(it.key().first));
        if (it.key().first > score)
        {
            score = it.key().first;
            best = it;
        }
    }

    // and use it
    if (best != edids.end())
    {
        gTorcEDID->m_edidData = best.value();
        gTorcEDID->Process();
    }
}

QByteArray TorcEDID::TrimEDID(const QByteArray &EDID)
{
    QByteArray result = EDID;

    QByteArray blank(128, 0);
    while ((result.size() > 128) && result.endsWith(blank))
        result.chop(128);

    return result;
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

QString TorcEDID::GetMSString(void)
{
    return m_productString;
}

void TorcEDID::Process(bool Quiet /*=false*/)
{
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
        LOG(VB_GENERAL, LOG_ERR, "EDID checksum error (AMD?)");
        return;
    }

    m_serialNumber  = qFromLittleEndian(*((quint32*)(m_edidData.data() + 12)));
    quint16 vendor  = qFromBigEndian(*((quint16*)(m_edidData.data() + 8)));
    quint16 code    = qFromLittleEndian(*((quint16*)(m_edidData.data() + 10)));
    QString codestr = (QString("%1%2").arg((quint8)((code >> 8) & 0xff), 2, 16, QLatin1Char('0')).arg((quint8)((code >> 0) & 0xff), 2, 16, QLatin1Char('0'))).toUpper();

    quint8 one   = (vendor >> 10) & 0x1F;
    quint8 two   = (vendor >> 05) & 0x1F;
    quint8 three = (vendor >> 00) & 0x1F;

    m_productString = QString("%1%2%3%4").arg(QLatin1Char(64 + one)).arg(QLatin1Char(64 + two)).arg(QLatin1Char(64 + three)).arg(codestr);

    static const char nameblock[4]   = {0, 0, 0, (char)252};
    static const char serialblock[4] = {0, 0, 0, (char)255};
    static const int  offsets[4]     = {54, 72, 90, 108};

    for (int i = 0; i < 4; ++i)
    {
        if (memcmp(nameblock, m_edidData.data() + offsets[i], 4) == 0)
            m_name = QString::fromLatin1(m_edidData.data() + offsets[i] + 5, 12).trimmed();
        else if (memcmp(serialblock, m_edidData.data() + offsets[i], 4) == 0)
            m_serialNumberStr = QString::fromLatin1(m_edidData.data() + offsets[i] + 5, 12).trimmed();
    }

    if (!Quiet)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Processing EDID for '%1', serial %2: %3bytes version: %4.%5 with %6 extensions")
            .arg(m_name).arg(m_serialNumberStr).arg(m_edidData.size())
            .arg((quint8)m_edidData.at(18)).arg((quint8)m_edidData.at(19)).arg((quint8)m_edidData.at(126)));
    }

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
                if (!Quiet)
                    LOG(VB_GUI, LOG_INFO, QString("HDMI VSDB size: %1").arg(length));

                if (length >= 5)
                {
                    m_physicalAddress = (m_edidData.at(i + 3) << 8) + m_edidData.at(i + 4);
                    if (!Quiet)
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

                            if (!Quiet)
                                LOG(VB_GUI, LOG_INFO, QString("HDMI latencies, audio: %1 video: %2").arg(m_audioLatency).arg(m_videoLatency));

                            if (ilatencies && length >= 12 && ((i + 11) < size))
                            {
                                uint8_t ivideo = m_edidData.at(i + 10);
                                if (ivideo > 0 && ivideo <= 251)
                                    m_interlacedVideoLatency = (ivideo - 1) * 2;

                                uint8_t iaudio = m_edidData.at(i + 11);
                                if (iaudio > 0 && iaudio <= 251)
                                    m_interlacedAudioLatency = (iaudio - 1) * 2;

                                if (!Quiet)
                                {
                                    LOG(VB_GUI, LOG_INFO, QString("HDMI interlaced latencies, audio: %1 video: %2")
                                    .arg(m_interlacedAudioLatency).arg(m_interlacedVideoLatency));
                                }
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

EDIDFactory* EDIDFactory::gEDIDFactory = NULL;

EDIDFactory::EDIDFactory()
{
    nextEDIDFactory = gEDIDFactory;
    gEDIDFactory = this;
}

EDIDFactory::~EDIDFactory()
{
}

EDIDFactory* EDIDFactory::GetEDIDFactory(void)
{
    return gEDIDFactory;
}

EDIDFactory* EDIDFactory::NextFactory(void) const
{
    return nextEDIDFactory;
}
