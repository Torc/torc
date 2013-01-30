// Qt
#include <QApplication>
#include <QDesktopWidget>

// Torc
#include "torclocaldefs.h"
#include "torccompat.h"
#include "torcedid.h"
#include "torclogging.h"
#include "../uidisplay.h"

#ifndef DISPLAY_DEVICE_ACTIVE
#define DISPLAY_DEVICE_ACTIVE 0x00000001
#endif

#include <SetupApi.h>

const GUID GUID_MONITOR = {0x4d36e96e, 0xe325, 0x11ce, {0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18}};

static void GetEDID(WId Window)
{
    MONITORINFOEX monitor;
    memset(&monitor, 0, sizeof(MONITORINFOEX));
    monitor.cbSize = sizeof(MONITORINFOEX);

    HMONITOR monitorid = MonitorFromWindow(Window, MONITOR_DEFAULTTONEAREST);
    GetMonitorInfo(monitorid, &monitor);

    DISPLAY_DEVICE device;
    memset(&device, 0, sizeof(DISPLAY_DEVICE));
    device.cb = sizeof(DISPLAY_DEVICE);
    DWORD devicenumber = 0;

    QByteArray edid;

    while (EnumDisplayDevices(monitor.szDevice, devicenumber, &device, 0))
    {
        if (!(device.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) &&
             (device.StateFlags & DISPLAY_DEVICE_ACTIVE))
        {
            QStringList ids  = QString(device.DeviceID).split("\\");
            int numberfromid = -1;

            if (ids.size())
            {
                int number = ids.last().toInt();
                if (number > 0 && number < 100)
                    numberfromid = number - 1;
            }

            HDEVINFO info = SetupDiGetClassDevsEx(&GUID_MONITOR, NULL, NULL, DIGCF_PRESENT, NULL, NULL, NULL);
            if (info)
            {
                for (int i = 0; ERROR_NO_MORE_ITEMS != GetLastError(); ++i)
                {
                    SP_DEVINFO_DATA infodata;
                    memset(&infodata, 0, sizeof(SP_DEVINFO_DATA));
                    infodata.cbSize = sizeof(infodata);

                    if (SetupDiEnumDeviceInfo(info, i, &infodata))
                    {
                        HKEY devicekey = SetupDiOpenDevRegKey(info, &infodata, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
                        if(!devicekey || (devicekey == INVALID_HANDLE_VALUE))
                            continue;

                        for (LONG i = 0, result = ERROR_SUCCESS; result != ERROR_NO_MORE_ITEMS; ++i)
                        {
                            DWORD entrylength = 128;
                            QByteArray entryname(entrylength, 0);
                            DWORD thisedidsize = 1024;
                            QByteArray thisedid(thisedidsize, 0);
                            DWORD dummy;
                            result = RegEnumValue(devicekey, i, (char*)entryname.data(), &entrylength, NULL, &dummy, (BYTE*)thisedid.data(), &thisedidsize);
                            if ((result == ERROR_SUCCESS) && entryname.contains("EDID"))
                            {
                                if (i == 0 || i == numberfromid)
                                    edid = thisedid;
                            }
                        }

                        RegCloseKey(devicekey);
                    }
                }

                SetupDiDestroyDeviceInfoList(info);
            }
        }

        devicenumber++;
        memset(&device, 0, sizeof(DISPLAY_DEVICE));
        device.cb = sizeof(DISPLAY_DEVICE);
    }

    if (!edid.isEmpty())
    {
        // trim any excess
        QByteArray blank(128, 0);
        while ((edid.size() > 128) && edid.endsWith(blank))
            edid.chop(128);
        TorcEDID::RegisterEDID(edid);
    }
}

UIDisplay::UIDisplay(QWidget *Widget)
  : UIDisplayBase(Widget)
{
}

UIDisplay::~UIDisplay()
{
}

bool UIDisplay::InitialiseDisplay(void)
{
    m_pixelSize    = GetGeometryPriv();
    m_physicalSize = GetPhysicalSizePriv();
    m_refreshRate  = GetRefreshRatePriv();
    m_screen       = GetScreenPriv();
    m_screenCount  = GetScreenCountPriv();

    Sanitise();

    return true;
}

static double RealRateFromInt(int Rate)
{
    // see http://support.microsoft.com/kb/2006076
    switch (Rate)
    {
        case 23:  return 23.976;
        case 29:  return 29.970;
        case 47:  return 47.952;
        case 59:  return 59.940;
        case 71:  return 71.928;
        case 119: return 119.880;
        case 61:  return 60.000;
        default:  return (double)Rate;
    }
}

static int IntRateFromRealRate(double Rate)
{
    if (qFuzzyCompare(23.976, Rate)) return 23;
    if (qFuzzyCompare(29.970, Rate)) return 29;
    if (qFuzzyCompare(47.952, Rate)) return 47;
    if (qFuzzyCompare(59.940, Rate)) return 59;
    if (qFuzzyCompare(71.928, Rate)) return 71;
    if (qFuzzyCompare(119.88, Rate)) return 119;
    if (qFuzzyCompare(60.000, Rate)) return 61;
    return (int)Rate;
}

void UIDisplay::SwitchToMode(int Index)
{
    if (Index < 0 || Index >= m_modes.size())
        return;

    DEVMODE mode;
    memset(&mode, 0, sizeof(DEVMODE));
    mode.dmSize = sizeof(DEVMODE);
    mode.dmDisplayFrequency = IntRateFromRealRate(m_modes[Index].m_rate);
    mode.dmBitsPerPel = m_modes[Index].m_depth;
    mode.dmFields = DM_DISPLAYFREQUENCY | DM_BITSPERPEL;

    if (m_modes[Index].m_interlaced)
    {
        mode.dmFields |= DM_DISPLAYFLAGS;
        mode.dmDisplayFlags = DM_INTERLACED;
    }

    LONG result = ChangeDisplaySettings(&mode, CDS_FULLSCREEN);
    if (DISP_CHANGE_SUCCESSFUL != result)
        LOG(VB_GENERAL, LOG_ERR, "Failed to set display refresh rate");
    else
        m_refreshRate = m_modes[Index].m_rate;
}

double UIDisplay::GetRefreshRatePriv(void)
{
    // current rate
    DEVMODE currentmode;
    memset(&currentmode, 0, sizeof(DEVMODE));
    currentmode.dmSize = sizeof(DEVMODE);

    double res = 0.0;

    if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &currentmode) &&
        (currentmode.dmFields & DM_DISPLAYFREQUENCY))
    {
        if (currentmode.dmDisplayFrequency == 61)
            m_variableRefreshRate = true;
        res = RealRateFromInt(currentmode.dmDisplayFrequency);
    }

    // check for EDID
    GetEDID(m_widget->winId());

    // other available rates
    m_modes.clear();
    m_originalModeIndex = -1;

    DEVMODE mode;
    memset(&mode, 0, sizeof(DEVMODE));
    mode.dmSize = sizeof(DEVMODE);
    DWORD modenumber = 0;
    int count = 0;

    while (EnumDisplaySettings(NULL, modenumber, &mode))
    {
        if (mode.dmFields & (DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY))
        {
            int modewidth   = mode.dmPelsWidth;
            int modeheight  = mode.dmPelsHeight;
            double moderate = RealRateFromInt(mode.dmDisplayFrequency);
            DWORD flags     = mode.dmFields & DM_DISPLAYFLAGS ? mode.dmDisplayFlags : 0;
            bool interlaced = flags & DM_INTERLACED;
            int depth       = mode.dmFields & DM_BITSPERPEL ? mode.dmBitsPerPel : 32;

            bool ignore = modewidth  != m_pixelSize.width() ||
                          modeheight != m_pixelSize.height() ||
                          depth < 32 || moderate < 10.0f || moderate > 121.0f;

            bool current = mode.dmBitsPerPel       == currentmode.dmBitsPerPel &&
                           mode.dmPelsWidth        == currentmode.dmPelsWidth &&
                           mode.dmPelsHeight       == currentmode.dmPelsHeight &&
                           mode.dmDisplayFrequency == currentmode.dmDisplayFrequency &&
                           mode.dmDisplayFlags     == currentmode.dmDisplayFlags;

            LOG(VB_GUI, LOG_INFO, QString("Mode %1x%2@%3Hz %4bpp%5%6%7")
                .arg(modewidth).arg(modeheight).arg(moderate).arg(depth)
                .arg(interlaced ? QString(" Interlaced") : "")
                .arg(ignore ? QString(" Ignoring") : "")
                .arg(current ? QString(" CURRENT"): ""));

            if (!ignore)
            {
                if (current)
                    m_originalModeIndex = m_modes.size();
                m_modes.append(UIDisplayMode(modewidth, modeheight, depth, moderate, interlaced, count++));
            }
        }

        memset(&mode, 0, sizeof(DEVMODE));
        mode.dmSize = sizeof(DEVMODE);
        modenumber++;
    }

    return res;
}

QSize UIDisplay::GetPhysicalSizePriv(void)
{
    HDC hdc = GetDC(m_widget->winId());

    if (hdc)
    {
        int width  = GetDeviceCaps(hdc, HORZSIZE);
        int height = GetDeviceCaps(hdc, VERTSIZE);

        ReleaseDC(m_widget->winId(), hdc);

        return QSize(width, height);
    }

    return QSize(-1, -1);
}
