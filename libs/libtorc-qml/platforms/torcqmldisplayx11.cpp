/* Class TorcQMLDisplayX11
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

// Qt
#include <QLibrary>

// Torc
#include "torclogging.h"
#include "torcedid.h"
#include "torcqmldisplayx11.h"

// X11
#include "nvctrl/torcnvcontrol.h"
extern "C" {
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/xf86vmode.h>
}
#ifndef V_INTERLACE
#define V_INTERLACE (0x010)
#endif
#include "adl/torcadl.h"

/// \cond
typedef XID RROutput;
typedef XID RRCrtc;
typedef XID RRMode;
typedef unsigned long  XRRModeFlags;
typedef unsigned short Rotation;
typedef unsigned short SizeID;

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
    char         *name;
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

typedef struct _XRRCrtcInfo
{
    Time         timestamp;
    int          x, y;
    unsigned int width, height;
    RRMode       mode;
    Rotation     rotation;
    int          noutput;
    RROutput    *outputs;
    Rotation     rotations;
    int          npossible;
    RROutput    *possible;
} XRRCrtcInfo;

typedef struct _XRRScreenConfiguration XRRScreenConfiguration;
/// \endcond

typedef bool                    (*XRandrQueryExtension)             (Display*, int*, int*);
typedef Status                  (*XRandrQueryVersion)               (Display*, int*, int*);
typedef XRRScreenResources*     (*XRandrGetScreenResources)         (Display*, Window);
typedef XRRScreenResources*     (*XRandrGetScreenResourcesCurrent)  (Display*, Window);
typedef void                    (*XRandrFreeScreenResources)        (XRRScreenResources*);
typedef XRRScreenConfiguration* (*XRandrGetScreenInfo)              (Display*, Window);
typedef void                    (*XRandrFreeScreenConfigInfo)       (XRRScreenConfiguration*);
typedef SizeID                  (*XRandrConfigCurrentConfiguration) (XRRScreenConfiguration*, Rotation*);
typedef Status                  (*XRandrSetScreenConfigAndRate)     (Display*, XRRScreenConfiguration*,
                                                                     Drawable, int, Rotation, short, Time);
typedef int                     (*XRandrGetOutputProperty)          (Display*, RROutput output, Atom,
                                                                     long, long, Bool, Bool, Atom,
                                                                     Atom*, int*, unsigned long*,
                                                                     unsigned long*, unsigned char **);
typedef XRRCrtcInfo*            (*XRandrGetCrtcInfo)                (Display*, XRRScreenResources*, RRCrtc);
typedef void                    (*XRandrFreeCrtcInfo)               (XRRCrtcInfo*);

static class TorcXRandr
{
  public:
    TorcXRandr()
      : m_valid(false),
        m_queryExtension(NULL),
        m_queryVersion(NULL),
        m_getScreenResources(NULL),
        m_freeScreenResources(NULL),
        m_getScreenInfo(NULL),
        m_freeScreenConfigInfo(NULL),
        m_configCurrentConfiguration(NULL),
        m_setScreenConfigAndRate(NULL),
        m_getOutputProperty(NULL),
        m_getScreenResourcesCurrent(NULL),
        m_getCRTCInfo(NULL),
        m_freeCRTCInfo(NULL)
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
        m_getOutputProperty          = (XRandrGetOutputProperty)          QLibrary::resolve("Xrandr", "XRRGetOutputProperty");
        m_getCRTCInfo                = (XRandrGetCrtcInfo)                QLibrary::resolve("Xrandr", "XRRGetCrtcInfo");
        m_freeCRTCInfo               = (XRandrFreeCrtcInfo)               QLibrary::resolve("Xrandr", "XRRFreeCrtcInfo");

        if (m_queryExtension && m_queryVersion && m_getScreenResources &&
            m_freeScreenResources && m_getScreenInfo && m_freeScreenConfigInfo &&
            m_configCurrentConfiguration && m_setScreenConfigAndRate && m_getOutputProperty &&
            m_getCRTCInfo && m_freeCRTCInfo)
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
                    {
                        if (minor > 2)
                            m_getScreenResourcesCurrent = (XRandrGetScreenResourcesCurrent)QLibrary::resolve("XRandr", "XRRGetScreenResourcesCurrent");
                        m_valid = true;
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_INFO, "Need at least version 1.2");
                    }
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
    XRandrGetOutputProperty          m_getOutputProperty;
    XRandrGetScreenResourcesCurrent  m_getScreenResourcesCurrent;
    XRandrGetCrtcInfo                m_getCRTCInfo;
    XRandrFreeCrtcInfo               m_freeCRTCInfo;

} TorcXRandr;

class TorcEDIDFactoryXrandr : public TorcEDIDFactory
{
    void GetEDID(QMap<QPair<int, QString>, QByteArray> &EDIDMap, QWindow *Window, int Screen)
    {
        if (!TorcXRandr.m_valid)
            return;

        const char *displaystring = NULL;
        Display* display = XOpenDisplay(displaystring);

        if (!display)
            return;

        int screen  = DefaultScreen(display);

        XRRScreenResources* screenresources = NULL;

        if (TorcXRandr.m_getScreenResourcesCurrent)
            screenresources = TorcXRandr.m_getScreenResourcesCurrent(display, RootWindow(display, screen));
        else
            screenresources = TorcXRandr.m_getScreenResources(display, RootWindow(display, screen));

        if (screenresources)
        {
            Atom atoms[] =
            {
                XInternAtom(display, "EDID", False),
                XInternAtom(display, "EDID_DATA", False),
                XInternAtom(display, "XFree86_DDC_EDID1_RAWDATA", False),
                0
            };

            for (int i = 0; i < screenresources->ncrtc; ++i)
            {
                XRRCrtcInfo* crtcinfo = TorcXRandr.m_getCRTCInfo(display, screenresources, screenresources->crtcs[i]);
                if (!crtcinfo)
                    continue;

                LOG(VB_GENERAL, LOG_INFO, QString("CRTC #%1 has %2 outputs").arg(i).arg(crtcinfo->noutput));

                if (crtcinfo->noutput >= 1)
                {
                    unsigned char* data         = NULL;
                    int actualformat            = 0;
                    unsigned long numberofitems = 0;
                    unsigned long bytesafter    = 0;
                    Atom actualtype             = 0;

                    for (int j = 0; j < 3; ++j)
                    {
                        if (TorcXRandr.m_getOutputProperty(display, crtcinfo->outputs[0], atoms[j],
                                                     0, 100, False, False,
                                                     AnyPropertyType,
                                                     &actualtype, &actualformat,
                                                     &numberofitems, &bytesafter, &data) == Success)
                        {
                            if (actualtype == XA_INTEGER && actualformat == 8 && numberofitems > 0 &&
                                (numberofitems % 128 == 0))
                            {
                                QByteArray edid((const char*)data, numberofitems);
                                EDIDMap.insert(qMakePair(50, QString("Xrandr")), edid);
                                break;
                            }
                        }
                    }
                }

                TorcXRandr.m_freeCRTCInfo(crtcinfo);
            }

            TorcXRandr.m_freeScreenResources(screenresources);
        }

        XCloseDisplay(display);
    }
} TorcEDIDFactoryXrandr;

TorcQMLDisplayX11::TorcQMLDisplayX11(QWindow *Window)
  : TorcQMLDisplay(Window)
{
    TorcXRandr.Init();
}

TorcQMLDisplayX11::~TorcQMLDisplayX11()
{
}

void TorcQMLDisplayX11::UpdateScreenData(void)
{
    if (!m_window || !m_screen)
        return;

    TorcQMLDisplay::UpdateScreenData();
    RefreshScreenModes();
    m_edid = TorcEDID::GetEDID(m_window, screenNumber);
    setScreenCECPhysicalAddress(m_edid.PhysicalAddress());
}

void TorcQMLDisplayX11::RefreshScreenModes(void)
{
    // clear old rates
    m_modes.clear();

    // open X display
    QByteArray displaystring = qgetenv("DISPLAY");
    Display* display = XOpenDisplay(displaystring.constData());

    if (!display)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to open X display '%1'.").arg(displaystring.constData()));
        return;
    }

    XF86VidModeModeLine mode_line;
    int dot_clock;
    int screen = DefaultScreen(display);

    if (XF86VidModeGetModeLine(display, screen, &dot_clock, &mode_line))
    {
        qreal currentrate = mode_line.htotal * mode_line.vtotal;
        bool interlaced = false;

        if (currentrate > 0.0 && dot_clock > 0)
        {
            currentrate = (dot_clock * 1000.0) / currentrate;
            if (((mode_line.flags & V_INTERLACE) != 0) && currentrate > 24.5 && currentrate < 30.5)
            {
                currentrate *= 2.0f;
                interlaced = true;
            }

            setScreenRefreshRate(currentrate);
            setScreenInterlaced(interlaced);
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

    if (TorcXRandr.m_valid)
    {
        // initialise TorcNVControl if available
        TorcNVControl::InitialiseMetaModes(display, DefaultScreen(display));

        XRRScreenResources *resources = TorcXRandr.m_getScreenResources(display, RootWindow(display, screen));
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
                bool sizematch  = mode.width == (uint)screenPixelSize.width() && mode.height == (uint)screenPixelSize.height();
                bool realinterlaced = false;
                double realrate = TorcNVControl::GetRateForMode(display, (int)(moderate + 0.5f), realinterlaced);

                if (realrate > 10.0f && realrate < 121.0f)
                {
                    ignore = !sizematch;
                    current = sizematch && qFuzzyCompare(realrate + 1.0f, screenRefreshRate + 1.0f) && (realinterlaced == interlaced);
                    LOG(VB_GENERAL, LOG_INFO, QString("nvidia Mode %1: %2x%3@%4%5%6%7")
                        .arg(mode.name).arg(mode.width).arg(mode.height).arg(realrate)
                        .arg(realinterlaced ? QString(" Interlaced") : "")
                        .arg(ignore ? QString(" Ignoring") : "")
                        .arg(current ? QString(" Current") : ""));

                    if (!ignore)
                        m_modes.append(TorcDisplayMode(mode.width, mode.height, 32, realrate, realinterlaced, i));
                }
                else
                {
                    ignore  = moderate < 10.0f || moderate > 121.0f || !sizematch;
                    current = sizematch && qFuzzyCompare(moderate + 1.0f, screenRefreshRate + 1.0f) && (screenInterlaced == interlaced);

                    LOG(VB_GENERAL, LOG_INFO, QString("Mode %1: %2x%3@%4%5%6%7")
                        .arg(mode.name).arg(mode.width).arg(mode.height).arg(moderate)
                        .arg(mode.modeFlags & V_INTERLACE ? QString(" Interlaced") : "")
                        .arg(ignore ? QString(" Ignoring") : "")
                        .arg(current ? QString(" Current") : ""));

                    if (!ignore)
                        m_modes.append(TorcDisplayMode(mode.width, mode.height, 32, moderate, mode.modeFlags & V_INTERLACE, i));
                }
            }

            TorcXRandr.m_freeScreenResources(resources);
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, "Need XRandr 1.2 or above to query available refresh rates");
        }
    }

    XCloseDisplay(display);
}

class TorcQMLDIsplayFactoryX11 : public TorcQMLDisplayFactory
{
    void ScoreTorcQMLDisplay(QWindow *Window, int &Score)
    {
        if (Score < 20)
            Score = 20;
    }

    TorcQMLDisplay* GetTorcQMLDisplay(QWindow *Window, int Score)
    {
        if (Score == 20)
            return new TorcQMLDisplayX11(Window);
        return NULL;
    }
} TorcQMLDIsplayFactoryX11;
