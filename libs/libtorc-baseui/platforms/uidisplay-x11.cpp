// Qt
#include <QLibrary>

// Torc
#include "torclogging.h"
#include "torcedid.h"
#include "uidisplay.h"

// X11
#include "nvctrl/uinvcontrol.h"
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
typedef unsigned long  XRRModeFlags;
typedef unsigned short Rotation;
typedef unsigned short	SizeID;

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

typedef struct _XRRScreenConfiguration XRRScreenConfiguration;

typedef bool                    (*XRandrQueryExtension)             (Display*, int*, int*);
typedef Status                  (*XRandrQueryVersion)               (Display*, int*, int*);
typedef XRRScreenResources*     (*XRandrGetScreenResources)         (Display*, Window);
typedef void                    (*XRandrFreeScreenResources)        (XRRScreenResources*);
typedef XRRScreenConfiguration* (*XRandrGetScreenInfo)              (Display*, Window);
typedef void                    (*XRandrFreeScreenConfigInfo)       (XRRScreenConfiguration*);
typedef SizeID                  (*XRandrConfigCurrentConfiguration) (XRRScreenConfiguration*, Rotation*);
typedef Status                  (*XRandrSetScreenConfigAndRate)     (Display*, XRRScreenConfiguration*,
                                                                     Drawable, int, Rotation, short, Time);
static class UIXRandr
{
  public:
    UIXRandr()
      : m_valid(false),
        m_queryExtension(NULL),
        m_queryVersion(NULL),
        m_getScreenResources(NULL),
        m_freeScreenResources(NULL),
        m_getScreenInfo(NULL),
        m_freeScreenConfigInfo(NULL),
        m_configCurrentConfiguration(NULL),
        m_setScreenConfigAndRate(NULL)
    {
    }

    void Init(void)
    {
        m_valid                      = false;
        m_queryExtension             = (XRandrQueryExtension)             QLibrary::resolve("Xrandr", "XRRQueryExtension");
        m_queryVersion               = (XRandrQueryVersion)               QLibrary::resolve("Xrandr", "XRRQueryVersion");
        m_getScreenResources         = (XRandrGetScreenResources)         QLibrary::resolve("Xrandr", "XRRGetScreenResources");
        m_freeScreenResources        = (XRandrFreeScreenResources)        QLibrary::resolve("Xrandr", "XRRFreeScreenResources");
        m_getScreenInfo              = (XRandrGetScreenInfo)              QLibrary::resolve("Xrandr", "XRRGetScreenInfo");
        m_freeScreenConfigInfo       = (XRandrFreeScreenConfigInfo)       QLibrary::resolve("Xrandr", "XRRFreeScreenConfigInfo");
        m_configCurrentConfiguration = (XRandrConfigCurrentConfiguration) QLibrary::resolve("Xrandr", "XRRConfigCurrentConfiguration");
        m_setScreenConfigAndRate     = (XRandrSetScreenConfigAndRate)     QLibrary::resolve("Xrandr", "XRRSetScreenConfigAndRate");

        if (m_queryExtension && m_queryVersion && m_getScreenResources &&
            m_freeScreenResources && m_getScreenInfo && m_freeScreenConfigInfo &&
            m_configCurrentConfiguration && m_setScreenConfigAndRate)
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

    bool                             m_valid;
    XRandrQueryExtension             m_queryExtension;
    XRandrQueryVersion               m_queryVersion;
    XRandrGetScreenResources         m_getScreenResources;
    XRandrFreeScreenResources        m_freeScreenResources;
    XRandrGetScreenInfo              m_getScreenInfo;
    XRandrFreeScreenConfigInfo       m_freeScreenConfigInfo;
    XRandrConfigCurrentConfiguration m_configCurrentConfiguration;
    XRandrSetScreenConfigAndRate     m_setScreenConfigAndRate;

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
    // TODO use display when needed
    const char *displaystring = NULL;
    Display* display = XOpenDisplay(displaystring);

    if (display)
    {
        if (UINVControl::NVControlAvailable(display))
        {
            int screen = DefaultScreen(display);
            UINVControl::InitialiseMetaModes(display, screen);
            TorcEDID::RegisterEDID(UINVControl::GetNVEDID(display, screen));
        }
        XCloseDisplay(display);
    }

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
    if (Index < 0 || Index >= m_modes.size())
        return;

    int index = m_modes[Index].m_index;
    if (index < 0)
        return;

    // TODO use display when needed
    Display* display = XOpenDisplay(NULL);

    if (UIXRandr.m_valid && display)
    {
        int screen  = DefaultScreen(display);
        XRRScreenConfiguration *config = UIXRandr.m_getScreenInfo(display, RootWindow(display, screen));

        if (config)
        {
            Rotation rotation;
            SizeID original = UIXRandr.m_configCurrentConfiguration(config, &rotation);
            XRRScreenResources *resources = UIXRandr.m_getScreenResources(display, RootWindow(display, screen));

            if (resources && (index < resources->nmode))
            {
                XRRModeInfo mode = resources->modes[index];
                double moderate = mode.hTotal * mode.vTotal;
                if (moderate > 0.0f && mode.dotClock > 0)
                    moderate = (double)mode.dotClock / moderate;
                short intrate = (short)(moderate + 0.5f);
                bool dummy;
                double realrate = UINVControl::GetRateForMode(display, intrate, dummy);
                if (realrate > 0.0f)
                    moderate = realrate;

                LOG(VB_GENERAL, LOG_INFO, QString("Trying %1Hz (%2Hz Index %3)").arg(moderate).arg(intrate).arg(index));
                if (!UIXRandr.m_setScreenConfigAndRate(display, config, RootWindow(display, screen), original, rotation, intrate, CurrentTime))
                    LOG(VB_GENERAL, LOG_ERR, "Failed to set video mode");

                UIXRandr.m_freeScreenResources(resources);
                XSync(display, false);
            }

            UIXRandr.m_freeScreenConfigInfo(config);
        }
    }

    XCloseDisplay(display);
}

double UIDisplay::GetRefreshRatePriv(void)
{
    double currentrate = -1;
    bool currentinterlaced = false;

    XF86VidModeModeLine mode_line;
    int dot_clock;

    // TODO use display when needed
    const char *displaystring = NULL;
    Display* display = XOpenDisplay(displaystring);

    if (!display)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to open X display.");
        return currentrate;
    }

    int screen = DefaultScreen(display);

    if (XF86VidModeGetModeLine(display, screen, &dot_clock, &mode_line))
    {
        currentrate = mode_line.htotal * mode_line.vtotal;

        if (currentrate > 0.0 && dot_clock > 0)
        {
            currentrate = (dot_clock * 1000.0) / currentrate;
            if (((mode_line.flags & V_INTERLACE) != 0) && currentrate > 24.5 && currentrate < 30.5)
            {
                currentrate *= 2.0f;
                currentinterlaced = true;
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
                bool interlaced = false;
                XRRModeInfo mode = resources->modes[i];
                double moderate = mode.hTotal * mode.vTotal;
                if (moderate > 0.0 && mode.dotClock > 0)
                {
                    moderate = mode.dotClock / moderate;
                    if ((mode.modeFlags & V_INTERLACE) && moderate > 24.5 && moderate < 30.5)
                    {
                        moderate *= 2.0f;
                        interlaced = true;
                    }
                }

                bool ignore     = false;
                bool current    = false;
                bool sizematch  = mode.width == (uint)m_pixelSize.width() || mode.height == (uint)m_pixelSize.height();
                bool realinterlaced = false;
                double realrate = UINVControl::GetRateForMode(display, (int)(moderate + 0.5f), realinterlaced);

                if (realrate > 10.0f && realrate < 121.0f)
                {
                    ignore = !sizematch;
                    current = sizematch && qFuzzyCompare(realrate + 1.0f, currentrate + 1.0f) && (realinterlaced == interlaced);
                    LOG(VB_GUI, LOG_INFO, QString("nvidia Mode %1: %2x%3@%4%5%6%7")
                        .arg(mode.name).arg(mode.width).arg(mode.height).arg(moderate)
                        .arg(realinterlaced ? QString(" Interlaced") : "")
                        .arg(ignore ? QString(" Ignoring") : "")
                        .arg(current ? QString(" Current") : ""));

                    if (!ignore)
                        m_modes.append(UIDisplayMode(mode.width, mode.height, 32, realrate, realinterlaced, i));
                }
                else
                {
                    ignore  = moderate < 10.0f || moderate > 121.0f || !sizematch;
                    current = sizematch && qFuzzyCompare(moderate + 1.0f, currentrate + 1.0f) && (currentinterlaced == interlaced);

                    LOG(VB_GUI, LOG_INFO, QString("Mode %1: %2x%3@%4%5%6%7")
                        .arg(mode.name).arg(mode.width).arg(mode.height).arg(moderate)
                        .arg(mode.modeFlags & V_INTERLACE ? QString(" Interlaced") : "")
                        .arg(ignore ? QString(" Ignoring") : "")
                        .arg(current ? QString(" Current") : ""));

                    if (!ignore)
                        m_modes.append(UIDisplayMode(mode.width, mode.height, 32, moderate, mode.modeFlags & V_INTERLACE, i));
                }

                if (current)
                    m_originalModeIndex = m_modes.size() - 1;
            }

            UIXRandr.m_freeScreenResources(resources);
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, "Need XRandr 1.2 or above to query available refresh rates");
        }
    }

    XCloseDisplay(display);

    return currentrate;
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
