// Std
#import <CoreFoundation/CFDictionary.h>

// Qt
#include <QApplication>
#include <QDesktopWidget>
#include <QWindowsStyle>

// Torc
#include "torclogging.h"
#include "torcedid.h"
#include "torccocoa.h"
#include "uidisplay.h"

CGDirectDisplayID GetOSXDisplay(WId win);

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

    {
        CocoaAutoReleasePool pool;
        CGDirectDisplayID disp = GetOSXDisplay(m_widget->winId());
        TorcEDID::RegisterEDID(GetOSXEDID(disp), false);
    }

    return true;
}

double UIDisplay::GetRefreshRatePriv(void)
{
    CocoaAutoReleasePool pool;

    double rate = 0.0f;
    CGDirectDisplayID disp = GetOSXDisplay(m_widget->winId());
    if (!disp)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Failed to get OS X display");
        return rate;
    }

    CFDictionaryRef ref = CGDisplayCurrentMode(disp);
    if (ref)
    {
        CFNumberRef number = (CFNumberRef)CFDictionaryGetValue(ref, kCGDisplayRefreshRate);
        if (number)
        {
            CFNumberGetValue(number, kCFNumberSInt32Type, &rate);
            if (qFuzzyCompare((double)1.0f, rate + 1.0f))
                m_variableRefreshRate = true;
        }
        else
            LOG(VB_GENERAL, LOG_WARNING, "Failed to get current display rate");
    }
    else
    {
        LOG(VB_GENERAL, LOG_WARNING, "Failed to get current display mode");
    }

    return rate;
}

QSize UIDisplay::GetPhysicalSizePriv(void)
{
    CocoaAutoReleasePool pool;

    QSize size(0, 0);

    CGDirectDisplayID disp = GetOSXDisplay(m_widget->winId());
    if (!disp)
        return size;

    CGSize size_in_mm = CGDisplayScreenSize(disp);
    return QSize((uint)size_in_mm.width, (uint)size_in_mm.height);
}

CGDirectDisplayID GetOSXDisplay(WId win)
{
    if (!win)
        return NULL;

#ifdef QT_MAC_USE_COCOA
    return GetOSXCocoaDisplay((void*)win);
#else
    CGDirectDisplayID disp = NULL;
    HIViewRef hiview = (HIViewRef)win;
    WindowRef winref = HIViewGetWindow(hiview);
    Rect bounds;
    if (!GetWindowBounds(winref, kWindowStructureRgn, &bounds))
    {
        CGDisplayCount ct;
        CGPoint pt;
        pt.x = bounds.left;
        pt.y = bounds.top;
        if (kCGErrorSuccess != CGGetDisplaysWithPoint(pt, 1, &disp, &ct))
            disp = CGMainDisplayID();
    }
    return disp;
#endif
}

