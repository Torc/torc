/* Class VideoVDPAU
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
#include "videovdpau.h"

extern "C" {
#include "libavcodec/vdpau.h"
}

static QMutex* gVDPAULock = new QMutex(QMutex::Recursive);

bool VideoVDPAU::VDPAUAvailable(void)
{
    QMutexLocker locker(gVDPAULock);

    static bool available = false;
    static bool checked   = false;

    if (checked)
        return available;

    checked = true;

    Display *display = XOpenDisplay(NULL);
    if (display)
    {
        VdpDevice device = VDP_STATUS_INVALID_HANDLE;
        VdpGetProcAddress *getprocaddress = NULL;
        VdpStatus status = vdp_device_create_x11(display, DefaultScreen(display), &device, &getprocaddress);

        if (status == VDP_STATUS_OK)
        {
            LOG(VB_GENERAL, LOG_INFO, "VDPAU hardware decoding available");
            available = true;
            VdpDeviceDestroy *destroy = NULL;
            status = getprocaddress(device, VDP_FUNC_ID_DEVICE_DESTROY, (void **)&destroy);
            if (status == VDP_STATUS_OK)
                destroy(device);
            else
                LOG(VB_GENERAL, LOG_WARNING, "Created VDPAU device but failed to find destructor function - leaking");
        }

        XCloseDisplay(display);
    }

    return available;
}
