/* Class TorcUSBPrivWin
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

//Torc
#include "torclocaldefs.h"
#include "torccompat.h"
#include "torclogging.h"
#include "torcusbprivwin.h"

#include <SetupApi.h>

static const GUID GUID_USB_RAW =  {0xA5DCBF10, 0x6530, 0x11D2, {0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED}};

TorcUSBPrivWin::TorcUSBPrivWin(TorcUSB *Parent)
  : QObject(Parent)
{
    Refresh();
}

TorcUSBPrivWin::~TorcUSBPrivWin()
{
}

void TorcUSBPrivWin::Destroy(void)
{
    deleteLater();
}

TorcUSBDevice DeviceFromPath(const QString &Path)
{
    QStringList paths = Path.split("\\");
    foreach (QString path, paths)
    {
        int vidpos = path.indexOf("vid_");
        int pidpos = path.indexOf("pid_");

        if (vidpos > -1 && pidpos > -1)
        {
            bool ok;
            int vid = path.mid(vidpos + 4, 4).toUInt(&ok, 16);
            if (ok)
            {
                int pid = path.mid(pidpos + 4, 4).toUInt(&ok, 16);
                if (ok)
                    return TorcUSBDevice(Path, vid, pid, TorcUSBDevice::Unknown);
            }
        }
    }

    return TorcUSBDevice(Path, 0, 0, TorcUSBDevice::Unknown);
}

void TorcUSBPrivWin::Refresh(void)
{
    HDEVINFO handle = SetupDiGetClassDevs(&GUID_USB_RAW, 0, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    if (INVALID_HANDLE_VALUE == handle)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Failed to get handle to retrieve USB devices");
        return;
    }

    DWORD index = 0;
    QStringList paths;

    while (1)
    {
        SP_DEVINFO_DATA deviceinfodata;
        deviceinfodata.cbSize = sizeof(SP_DEVINFO_DATA);

        if (SetupDiEnumDeviceInfo(handle, index, &deviceinfodata))
        {
            SP_DEVICE_INTERFACE_DATA deviceinterfacedata;
            deviceinterfacedata.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

            if (SetupDiEnumDeviceInterfaces(handle, 0, &GUID_USB_RAW, index, &deviceinterfacedata))
            {
                index++;

                DWORD size = 0;

                SetupDiGetDeviceInterfaceDetail(handle, &deviceinterfacedata, NULL, 0, &size, NULL);
                PSP_DEVICE_INTERFACE_DETAIL_DATA deviceinterfacedetaildata = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(size * sizeof(TCHAR));
                deviceinterfacedetaildata->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA);

                DWORD datasize = size;

                if (SetupDiGetDeviceInterfaceDetail(handle, &deviceinterfacedata, deviceinterfacedetaildata, datasize , &size, NULL))
                    paths.append(deviceinterfacedetaildata->DevicePath);

                free(deviceinterfacedetaildata);
            }
        }
        else
        {
            break;
        }
    }

    SetupDiDestroyDeviceInfoList(handle);

    // add new devices
    foreach (QString path, paths)
    {
        if (!m_devicePaths.contains(path))
        {
            m_devicePaths.append(path);
            ((TorcUSB*)parent())->DeviceAdded(DeviceFromPath(path));
        }
    }

    // and remove old
    QStringList stale;
    foreach (QString path, m_devicePaths)
        if (!paths.contains(path))
            stale.append(path);

    foreach (QString path, stale)
    {
        m_devicePaths.removeAll(path);
        ((TorcUSB*)parent())->DeviceRemoved(DeviceFromPath(path));
    }
}

class USBFactoryWin : public USBFactory
{
    void Score(int &Score)
    {
        if (Score <= 10)
            Score = 10;
    }

    TorcUSBPriv* Create(int Score, TorcUSB *Parent)
    {
        (void)Parent;

        if (Score <= 10)
            return new TorcUSBPrivWin(Parent);

        return NULL;
    }
} USBFactoryWin;



