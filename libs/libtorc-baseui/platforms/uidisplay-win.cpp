// Qt
#include <QApplication>
#include <QDesktopWidget>

// Torc
#include "torccompat.h"
#include "torclogging.h"
#include "../uidisplay.h"

#define VALID_RATE(rate) (rate > 20.0 && rate < 200.0)

#define LOC QString("WinDisplay: ")

static qreal fix_rate(qreal video_rate)
{
    static const qreal default_rate = 60.0;
    qreal fixed = default_rate;
    if (video_rate > 0 && video_rate < default_rate)
        fixed = default_rate;
    return fixed;
}

UIDisplay::UIDisplay(QWidget *Widget)
    : UIDisplayBase(Widget),
      m_physicalSize(-1, -1),
      m_refreshRate(-1.0)
{
}

UIDisplay::~UIDisplay()
{
}

bool UIDisplay::InitialiseDisplay(void)
{
    m_pixelSize    = GetGeometry();
    m_physicalSize = GetPhysicalSize();
    m_refreshRate  = GetRefreshRate();
    m_screen       = GetScreen();
    m_screenCount  = GetScreenCount();

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Using screen %1 of %2")
        .arg(m_screen + 1).arg(m_screenCount));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Refresh rate: %1Hz").arg(m_refreshRate));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Screen size : %1mm x %2mm")
        .arg(m_physicalSize.width()).arg(m_physicalSize.height()));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Screen size : %1px x %2px")
        .arg(m_pixelSize.width()).arg(m_pixelSize.height()));

    return true;
}

qreal UIDisplay::GetRefreshRate(void)
{
    HDC hdc = GetDC(m_widget->winId());

    qreal res = 0.0;
    int rate = 0;
    if (hdc)
    {
        rate = GetDeviceCaps(hdc, VREFRESH);
        if (VALID_RATE(rate))
        {
            // see http://support.microsoft.com/kb/2006076
            switch (rate)
            {
                case 23:  res = 23.976;  break;
                case 29:  res = 29.970;  break;
                case 47:  res = 47.952;  break;
                case 59:  res = 59.940;  break;
                case 71:  res = 71.928;  break;
                case 119: res = 119.880; break;
                default:  res = (qreal)rate;
            }
        }
        else
            res = fix_rate(rate);
    }

    return res;
}

QSize UIDisplay::GetPhysicalSize(void)
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
