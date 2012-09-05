// Std
#import <CoreFoundation/CFDictionary.h>

// Qt
#include <QApplication>
#include <QDesktopWidget>
#include <QWindowsStyle>

// Torc
#include "torclogging.h"
#include "torccocoa.h"
#include "uidisplay.h"

#define VALID_RATE(rate) (rate > 20.0 && rate < 200.0)

CGDirectDisplayID GetOSXDisplay(WId win);

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
    CocoaAutoReleasePool pool;

    qreal rate = 0.0f;
    CGDirectDisplayID disp = GetOSXDisplay(m_widget->winId());
    if (!disp)
        return rate;

    CFDictionaryRef ref = CGDisplayCurrentMode(disp);
    if (ref)
    {
        CFNumberRef number = (CFNumberRef)CFDictionaryGetValue(ref, kCGDisplayRefreshRate);
        if (number)
            CFNumberGetValue(number, kCFNumberSInt32Type, &rate);
    }

    if (!(VALID_RATE(rate)))
        rate = fix_rate(rate);

    return rate;
}

QSize UIDisplay::GetPhysicalSize(void)
{
    CocoaAutoReleasePool pool;

    QSize size(0, 0);

    CGDirectDisplayID disp = GetOSXDisplay(m_widget->winId());
    if (!disp)
        return size;

    CGSize size_in_mm = CGDisplayScreenSize(disp);
    return QSize((uint) size_in_mm.width, (uint) size_in_mm.height);
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

