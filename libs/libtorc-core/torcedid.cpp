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
 *
 * Depending upon the operating system in use, grabbing the EDID may require
 * a graphics server to be running. Hence implementations may submit a provisional
 * EDID which will only be used if a further update is not provided when the
 * the GUI is created. This revised EDID may provide more accurate data on systems
 * with multiple connected displays. (EDID data can still be of use to a command
 * line application that outputs audio of an HDMI link).
*/

TorcEDID::TorcEDID()
  : m_edidDataProvisional(true),
    m_physicalAddress(0x0000)
{
}

TorcEDID::~TorcEDID()
{
}

bool TorcEDID::Ready(void)
{
    QMutexLocker locker(gTorcEDIDLock);

    return !gTorcEDID->m_edidData.isEmpty() && !gTorcEDID->m_edidDataProvisional;
}

bool TorcEDID::Provisional(void)
{
    QMutexLocker locker(gTorcEDIDLock);

    return !gTorcEDID->m_edidDataProvisional;
}

void TorcEDID::RegisterEDID(QByteArray Data, bool Provisional)
{
    QMutexLocker locker(gTorcEDIDLock);

    if (!Data.isEmpty())
    {
        gTorcEDID->m_edidDataProvisional = Provisional;
        gTorcEDID->m_edidData = Data;
        gTorcEDID->Process();
    }
}

qint16 TorcEDID::PhysicalAddress(void)
{
    QMutexLocker locker(gTorcEDIDLock);

    return gTorcEDID->m_physicalAddress;
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

    LOG(VB_GENERAL, LOG_INFO, QString("Processing %1EDID")
        .arg(m_edidDataProvisional ? "provisional " : ""));

    // reset
    m_physicalAddress = 0x0000;

    // extensions
    if (m_edidData.size() > 128)
    {
        for (int i = 0; i < m_edidData.size(); ++i)
        {
            if (m_edidData.at(i)     == 0x03 &&
                m_edidData.at(i + 1) == 0x0C &&
                m_edidData.at(i + 2) == 0x0)
            {
              m_physicalAddress = (m_edidData.at(i + 3) << 8) + m_edidData.at(i + 4);
              LOG(VB_GENERAL, LOG_INFO, QString("EDID physical address 0x%1")
                  .arg(m_physicalAddress,0,16));
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
