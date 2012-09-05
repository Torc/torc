// Torc
#include "torclogging.h"
#include "uidisplay.h"

// X11
#include <X11/Xlib.h>
#ifndef V_INTERLACE
#define V_INTERLACE (0x010)
#endif
extern "C" {
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/xf86vmode.h>
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

    LOG(VB_GENERAL, LOG_INFO, QString("Using screen %1 of %2")
        .arg(m_screen + 1).arg(m_screenCount));
    LOG(VB_GENERAL, LOG_INFO, QString("Refresh rate: %1Hz").arg(m_refreshRate));
    LOG(VB_GENERAL, LOG_INFO, QString("Screen size : %1mm x %2mm")
        .arg(m_physicalSize.width()).arg(m_physicalSize.height()));
    LOG(VB_GENERAL, LOG_INFO, QString("Screen size : %1px x %2px")
        .arg(m_pixelSize.width()).arg(m_pixelSize.height()));

    return true;
}

qreal UIDisplay::GetRefreshRate(void)
{
    qreal rate = -1;

    XF86VidModeModeLine mode_line;
    int dot_clock;

    // TODO use display when needed
    const char *displaystring = NULL;
    Display* display = XOpenDisplay(displaystring);

    if (display)
    {
        int screen  = DefaultScreen(display);

        if (XF86VidModeGetModeLine(display, screen, &dot_clock, &mode_line))
        {
            rate = mode_line.htotal * mode_line.vtotal;

            if (rate > 0.0 && dot_clock > 0)
            {
                rate = (dot_clock * 1000.0) / rate;

                if (((mode_line.flags & V_INTERLACE) != 0) &&
                    rate > 24.5 && rate < 30.5)
                {
                    LOG(VB_PLAYBACK, LOG_INFO,
                            "Doubling refresh rate for interlaced display.");
                    rate *= 2.0;
                }

            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, "Modeline query returned zeroes");
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to get modeline.");
        }

        XCloseDisplay(display);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to open X display.");
    }

    return rate;
}

QSize UIDisplay::GetPhysicalSize(void)
{
    int displayWidthMM  = 400;
    int displayHeightMM = 225;

    // TODO use display when needed
    const char *displaystring = NULL;
    Display* display = XOpenDisplay(displaystring);

    if (display)
    {
        int screen  = DefaultScreen(display);
        displayWidthMM  = DisplayWidthMM (display, screen);
        displayHeightMM = DisplayHeightMM(display, screen);
        XCloseDisplay(display);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to open X display.");
    }

    return QSize(displayWidthMM, displayHeightMM);
}
