/* Class TorcCECDevice/TorcCECDeviceHandler
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

// Torc
#include "torclogging.h"
#include "torcusb.h"
#include "torccecdevice.h"

TorcCECDevice::TorcCECDevice()
{
}

static class TorcCECDeviceHandler : public TorcUSBDeviceHandler
{
  public:
    bool DeviceAdded(const TorcUSBDevice &Device)
    {
        if (!m_path.isEmpty())
        {
            LOG(VB_GENERAL, LOG_NOTICE, "Already have one CEC device attached - ignoring");
            return false;
        }

        if (Device.m_vendorID  == 0x2548 && Device.m_productID == 0x1001)
        {
            m_path = Device.m_path;
            LOG(VB_GENERAL, LOG_INFO, "CEC device added");
            return true;
        }

        return false;
    }

    bool DeviceRemoved(const TorcUSBDevice &Device)
    {
        if (Device.m_path == m_path)
        {
            LOG(VB_GENERAL, LOG_INFO, "CEC device removed");
            m_path = QString("");
            return true;
        }

        return false;
    }

  private:
    QString m_path;

} TorcCECDeviceHandler;
