// Qt
#include <QApplication>
#include <QDesktopWidget>

// Torc
#include "torccompat.h"
#include "../uidisplay.h"

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

double UIDisplay::GetRefreshRatePriv(void)
{
    HDC hdc = GetDC(m_widget->winId());

    double res = 0.0;
    int rate = 0;
    if (hdc)
    {
        rate = GetDeviceCaps(hdc, VREFRESH);
        // see http://support.microsoft.com/kb/2006076
        switch (rate)
        {
            case 23:  res = 23.976;  break;
            case 29:  res = 29.970;  break;
            case 47:  res = 47.952;  break;
            case 59:  res = 59.940;  break;
            case 71:  res = 71.928;  break;
            case 119: res = 119.880; break;
            default:  res = (double)rate;
        }
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
        return QSize(width, height);
    }

    return QSize(-1, -1);
}
