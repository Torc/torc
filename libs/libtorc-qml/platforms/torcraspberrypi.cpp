/* Clas TorcRaspberryPi
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2013
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
#include "torclocaldefs.h"
#include "torccompat.h"
#include "torclogging.h"
#include "torcadminthread.h"
#include "torcusb.h"
#include "torcedid.h"
#include "torcraspberrypi.h"

// Pi
#include "bcm_host.h"
#include "interface/vchiq_arm/vchiq_if.h"
#include "interface/vmcs_host/vc_tvservice.h"

bool InitialiseRaspberryPi(void)
{
    static bool initialised = false;
    static bool valid       = false;
    if (initialised)
        return valid;

    initialised = true;

    // initialise top level
    bcm_host_init();

    // initialise tv service
    static VCHI_INSTANCE_T    instance;
    static VCHI_CONNECTION_T *connections;

    vcos_init();
    if (vchi_initialise(&instance) == VCHIQ_SUCCESS)
    {
        if (vchi_connect(NULL, 0, instance) == VCHIQ_SUCCESS)
        {
            if (vc_vchi_tv_init(instance, &connections, 1) == VCHIQ_SUCCESS)
            {
                valid = true;
                char version[128];
                vc_gencmd(version, sizeof(version), "version");
                LOG(VB_GENERAL, LOG_INFO, QString("Initialised RaspberryPi, firmware version: %1").arg(version));
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, "Failed to initialise tv service");
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to connect to VCHI");
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to initialise VCHI");
    }

    if (!valid)
        LOG(VB_GENERAL, LOG_ERR, "Failed to initialise RaspberryPi hardware");

    return valid;
}

static class TorcPiCEC : public TorcAdminObject
{
  public:
    TorcPiCEC()
      : TorcAdminObject(TORC_ADMIN_LOW_PRIORITY)
    {
    }

    void Create(void)
    {
        TorcUSBDevice dummy("dummy", 0x2548, 0x1001, TorcUSBDevice::Unknown);
        TorcUSBDeviceHandler::DeviceHandled(dummy, true);
    }
    void Destroy(void)
    {
        TorcUSBDevice dummy("dummy", 0x2548, 0x1001, TorcUSBDevice::Unknown);
        TorcUSBDeviceHandler::DeviceHandled(dummy, false);
    }
} TorcPiCEC;

class EDIDFactoryRaspberryPi : public EDIDFactory
{
    void GetEDID(QMap<QPair<int,QString>,QByteArray > &EDIDMap, WId Window, int Screen)
    {
        if (!InitialiseRaspberryPi())
            return;

        (void)Window;
        (void)Screen;

        QByteArray edid;
        int offset = 0;
        uint8_t buffer[128];
        int buffersize = sizeof(buffer);

        while (vc_tv_hdmi_ddc_read(offset, buffersize, buffer) == buffersize)
        {
            offset += buffersize;
            edid.append(QByteArray((const char*)buffer, buffersize));
        }

        if (!edid.isEmpty())
            EDIDMap.insert(qMakePair(90, QString("RaspberryPi")), edid);
    }
} EDIDFactoryRaspberryPi;
