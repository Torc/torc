// Qt
#include <QLibrary>

// Torc
#include "torclogging.h"
#include "uidisplay.h"

// X11
extern "C" {
#include <X11/Xlib.h>
#include <X11/extensions/xf86vmode.h>
}
#ifndef V_INTERLACE
#define V_INTERLACE (0x010)
#endif

typedef XID RROutput;
typedef XID RRCrtc;
typedef XID RRMode;

typedef unsigned long XRRModeFlags;

typedef struct _XRRModeInfo
{
    RRMode        id;
    unsigned int  width;
    unsigned int  height;
    unsigned long dotClock;
    unsigned int  hSyncStart;
    unsigned int  hSyncEnd;
    unsigned int  hTotal;
    unsigned int  hSkew;
    unsigned int  vSyncStart;
    unsigned int  vSyncEnd;
    unsigned int  vTotal;
    char		 *name;
    unsigned int  nameLength;
    XRRModeFlags  modeFlags;
} XRRModeInfo;

typedef struct _XRRScreenResources
{
    Time         timestamp;
    Time         configTimestamp;
    int          ncrtc;
    RRCrtc      *crtcs;
    int          noutput;
    RROutput    *outputs;
    int          nmode;
    XRRModeInfo *modes;
} XRRScreenResources;

typedef bool                (*XRandrQueryExtension)     (Display*, int*, int*);
typedef Status              (*XRandrQueryVersion)       (Display*, int*, int*);
typedef XRRScreenResources* (*XRandrGetScreenResources) (Display*, Window);

static class UIXRandr
{
  public:
    UIXRandr()
      : m_valid(false),
        m_queryExtension(NULL),
        m_queryVersion(NULL),
        m_getScreenResources(NULL)
    {
    }

    void Init(void)
    {
        m_valid              = false;
        m_queryExtension     = (XRandrQueryExtension)     QLibrary::resolve("Xrandr", "XRRQueryExtension");
        m_queryVersion       = (XRandrQueryVersion)       QLibrary::resolve("Xrandr", "XRRQueryVersion");
        m_getScreenResources = (XRandrGetScreenResources) QLibrary::resolve("Xrandr", "XRRGetScreenResources");

        if (m_queryExtension && m_queryVersion && m_getScreenResources)
        {
            int event = 0;
            int error = 0;
            Display* display = XOpenDisplay(NULL);
            if (display && m_queryExtension(display, &event, &error))
            {
                int major = 0;
                int minor = 0;
                if ((*m_queryVersion)(display, &major, &minor))
                {
                    LOG(VB_GENERAL, LOG_INFO, QString("XRandR version: %1.%2").arg(major).arg(minor));
                    if ((major == 1 && minor >= 2) || (major > 1))
                        m_valid = true;
                    else
                        LOG(VB_GENERAL, LOG_INFO, "Need at least version 1.2");
                }
            }
        }
    }

    bool                     m_valid;
    XRandrQueryExtension     m_queryExtension;
    XRandrQueryVersion       m_queryVersion;
    XRandrGetScreenResources m_getScreenResources;

} UIXRandr;

UIDisplay::UIDisplay(QWidget *Widget)
  : UIDisplayBase(Widget)
{
    UIXRandr.Init();
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

void UIDisplay::SwitchToMode(int Index)
{
}

double UIDisplay::GetRefreshRatePriv(void)
{
    double rate = -1;

    XF86VidModeModeLine mode_line;
    int dot_clock;

    // TODO use display when needed
    const char *displaystring = NULL;
    Display* display = XOpenDisplay(displaystring);

    if (!display)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to open X display.");
        return rate;
    }

    int screen  = DefaultScreen(display);

    if (XF86VidModeGetModeLine(display, screen, &dot_clock, &mode_line))
    {
        rate = mode_line.htotal * mode_line.vtotal;

        if (rate > 0.0 && dot_clock > 0)
        {
            rate = (dot_clock * 1000.0) / rate;
            if (((mode_line.flags & V_INTERLACE) != 0) && rate > 24.5 && rate < 30.5)
                rate *= 2.0f;
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

    // all rates
    m_modes.clear();
    m_originalModeIndex = -1;

    if (UIXRandr.m_valid)
    {
        XRRScreenResources *resources = UIXRandr.m_getScreenResources(display, RootWindow(display, screen));
        if (resources)
        {
            for (int i = 0; i < resources->nmode; ++i)
            {
                XRRModeInfo mode = resources->modes[i];
                double moderate = mode.hTotal * mode.vTotal;
                if (moderate > 0.0 && mode.dotClock > 0)
                {
                    moderate = mode.dotClock / moderate;
                    if ((mode.modeFlags & V_INTERLACE) && moderate > 24.5 && moderate < 30.5)
                        moderate *= 2.0f;
                }

                bool ignore = moderate < 10.0f || moderate > 121.0f ||
                              mode.width  != (uint)m_pixelSize.width() ||
                              mode.height != (uint)m_pixelSize.height();

                LOG(VB_GUI, LOG_INFO, QString("Mode %1: %2x%3@%4%5%6")
                    .arg(mode.name).arg(mode.width).arg(mode.height).arg(moderate)
                    .arg(mode.modeFlags & V_INTERLACE ? QString(" Interlaced") : "")
                    .arg(ignore ? QString(" Ignoring") : ""));

                if (!ignore)
                {
                    m_modes.append(UIDisplayMode(mode.width, mode.height, 32, moderate, mode.modeFlags & V_INTERLACE, i));
                    if (qFuzzyCompare(moderate + 1.0f, rate + 1.0f))
                        m_originalModeIndex = i;
                }
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, "Need XRandr 1.2 or above to query available refresh rates");
        }
    }

    XCloseDisplay(display);

    return rate;
}

QSize UIDisplay::GetPhysicalSizePriv(void)
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
