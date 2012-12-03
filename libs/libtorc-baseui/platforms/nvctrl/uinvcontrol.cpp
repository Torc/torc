/* UINVControl
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
* You should have received a copy of the GNU General Public License
*
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Torc
#include "torclogging.h"
#include "uinvcontrol.h"

extern "C" {
#include "NVCtrl.h"
#include "NVCtrlLib.h"
}

QMutex* gNVCtrlLock = new QMutex(QMutex::Recursive);

bool UINVControl::NVControlAvailable(void)
{
    QMutexLocker locker(gNVCtrlLock);

    static bool available = false;
    static bool checked   = false;

    if (checked)
        return available;

    checked = true;

    const char* displayname = NULL;
    Display* display = XOpenDisplay(displayname);
    if (!display)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to open display '%1'").arg(XDisplayName(displayname)));
        return available;
    }

    int event = 0;
    int error = 0;
    Bool ok = XNVCTRLQueryExtension(display, &event, &error);

    if (ok != True)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("NV-CONTROL X extension not available on display '%1'").arg(XDisplayName(displayname)));
        return available;
    }

    int major = 0;
    int minor = 0;
    ok = XNVCTRLQueryVersion(display, &major, &minor);

    if (ok != True)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("NV-CONTROL X extension not available on display '%1'").arg(XDisplayName(displayname)));
        return available;
    }

    available = true;

    LOG(VB_GENERAL, LOG_INFO, QString("NV-CONTROL X extension version %1.%2 available on display '%3'")
        .arg(major).arg(minor).arg(XDisplayName(displayname)));

    return available;
}

QByteArray UINVControl::GetNVEDID(Display *XDisplay, int Screen)
{
    if (!NVControlAvailable() || !XDisplay)
        return QByteArray();

    QMutexLocker locker(gNVCtrlLock);
    QByteArray result;

    if (!XNVCTRLIsNvScreen(XDisplay, Screen))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("NV-CONTROL is not available on screen %1 of display '%2'")
            .arg(Screen).arg(XDisplayName(NULL)));
        return result;
    }

    unsigned char* data = NULL;
    int datalength = 0;

    Bool ok = XNVCTRLQueryBinaryData(XDisplay, Screen, 0, NV_CTRL_BINARY_DATA_EDID, &data, &datalength);

    if (ok != True)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("EDID not available on screen %1 of display '%2'")
                .arg(Screen).arg(XDisplayName(NULL)));
        return result;
    }

    result = QByteArray((const char*)data, datalength);
    return result;
}
