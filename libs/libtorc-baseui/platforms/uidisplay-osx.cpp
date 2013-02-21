// Std
#import <CoreFoundation/CFDictionary.h>

// Qt
#include <QApplication>
#include <QDesktopWidget>
#if (QT_VERSION >= 0x050000)
#include <QProxyStyle>
#else
#include <QWindowsStyle>
#endif

// Torc
#include "torclogging.h"
#include "uiedid.h"
#include "torcosxutils.h"
#include "torccocoa.h"
#include "uidisplay.h"

CGDirectDisplayID GetOSXDisplay(WId win);
int               DepthFromStringRef(CFStringRef Format);

static CFArrayRef gDisplayModes = NULL;

UIDisplay::UIDisplay(QWidget *Widget)
  : UIDisplayBase(Widget)
{
}

UIDisplay::~UIDisplay()
{
    if (gDisplayModes)
        CFRelease(gDisplayModes);
}

bool UIDisplay::InitialiseDisplay(void)
{
    m_pixelSize    = GetGeometryPriv();
    m_physicalSize = GetPhysicalSizePriv();
    m_refreshRate  = GetRefreshRatePriv();
    m_screen       = GetScreenPriv();
    m_screenCount  = GetScreenCountPriv();

    Sanitise();

    UIEDID::RegisterEDID(m_widget->winId(), m_screen);

    return true;
}

void UIDisplay::SwitchToMode(int Index)
{
    if (Index < 0 || Index >= m_modes.size() || !gDisplayModes)
        return;

    int index = m_modes[Index].m_index;

    if (index < 0 || index >= CFArrayGetCount(gDisplayModes))
        return;

    CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(gDisplayModes, index);

    CGDisplayConfigRef config;
    CGBeginDisplayConfiguration(&config);
    CGDirectDisplayID display = GetOSXDisplay(m_widget->winId());
    CGConfigureDisplayFadeEffect(config, 0.3f, 0.5f, 0, 0, 0);
    CGConfigureDisplayWithDisplayMode(config, display, mode, NULL);
    if (CGCompleteDisplayConfiguration(config, kCGConfigureForAppOnly))
        LOG(VB_GENERAL, LOG_ERR, "Failed to complete display configuration");
    else
        m_refreshRate = m_modes[Index].m_rate;
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

    CGDisplayModeRef current = CGDisplayCopyDisplayMode(disp);
    if (current)
    {
        rate = CGDisplayModeGetRefreshRate(current);
        if (qFuzzyCompare((double)1.0f, rate + 1.0f))
            m_variableRefreshRate = true;
    }
    else
    {
        LOG(VB_GENERAL, LOG_WARNING, "Failed to get current display mode");
        return rate;
    }

    if (m_variableRefreshRate)
    {
        CGDisplayModeRelease(current);
        return rate;
    }

    // if we've run this already, release the modes
    m_modes.clear();
    m_originalModeIndex = -1;
    if (gDisplayModes)
        CFRelease(gDisplayModes);

    gDisplayModes = CGDisplayCopyAllDisplayModes(disp, NULL);
    if (gDisplayModes)
    {
        for (int i = 0; i < CFArrayGetCount(gDisplayModes); ++i)
        {
            CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(gDisplayModes, i);
            int modewidth         = CGDisplayModeGetWidth(mode);
            int modeheight        = CGDisplayModeGetHeight(mode);
            double moderate       = CGDisplayModeGetRefreshRate(mode);
            int32_t flags         = CGDisplayModeGetIOFlags(mode);
            bool interlaced       = flags & kDisplayModeInterlacedFlag;
            CFStringRef fmt       = CGDisplayModeCopyPixelEncoding(mode);
            int depth             = DepthFromStringRef(fmt);
            CFRelease(fmt);

            bool ignore = modewidth  != m_pixelSize.width() ||
                          modeheight != m_pixelSize.height() ||
                          (flags & kDisplayModeNotGraphicsQualityFlag) ||
                         !(flags & kDisplayModeSafetyFlags) ||
                          depth < 32 || moderate < 10.0f || moderate > 121.0f;

            LOG(VB_GUI, LOG_INFO, QString("Mode %1x%2@%3Hz %4bpp%5%6%7")
                .arg(modewidth).arg(modeheight).arg(moderate).arg(depth)
                .arg(interlaced ? QString(" Interlaced") : "")
                .arg(ignore ? QString(" Ignoring") : "")
                .arg(mode == current ? QString(" CURRENT"): ""));

            if (!ignore)
            {
                m_modes.append(UIDisplayMode(modewidth, modeheight, depth, moderate, interlaced, i));
                if (mode == current)
                    m_originalModeIndex = m_modes.size() - 1;
            }
        }
    }

    CGDisplayModeRelease(current);
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

#if defined(QT_MAC_USE_COCOA) || (QT_VERSION >= 0x050000)
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

int DepthFromStringRef(CFStringRef Format)
{
    if (kCFCompareEqualTo == CFStringCompare(Format, CFSTR(kIO32BitFloatPixels), kCFCompareCaseInsensitive))
        return 96;
    else if (kCFCompareEqualTo == CFStringCompare(Format, CFSTR(kIO64BitDirectPixels), kCFCompareCaseInsensitive))
        return 64;
    else if (kCFCompareEqualTo == CFStringCompare(Format, CFSTR(kIO16BitFloatPixels), kCFCompareCaseInsensitive))
        return 48;
    else if (kCFCompareEqualTo == CFStringCompare(Format, CFSTR(IO32BitDirectPixels), kCFCompareCaseInsensitive))
        return 32;
    else if (kCFCompareEqualTo == CFStringCompare(Format, CFSTR(kIO30BitDirectPixels), kCFCompareCaseInsensitive))
        return 30;
    else if (kCFCompareEqualTo == CFStringCompare(Format, CFSTR(IO16BitDirectPixels), kCFCompareCaseInsensitive))
        return 16;
    else if (kCFCompareEqualTo == CFStringCompare(Format, CFSTR(IO8BitIndexedPixels), kCFCompareCaseInsensitive))
        return 8;
    return 0;
}

class EDIDFactoryOSX : public EDIDFactory
{
    void GetEDID(QMap<QPair<int, QString>, QByteArray> &EDIDMap, WId Window, int Screen)
    {
        (void)Screen;

        CocoaAutoReleasePool pool;
        CGDirectDisplayID disp = GetOSXDisplay(Window);
        QByteArray edid = GetOSXEDID(disp);
        if (!edid.isEmpty())
            EDIDMap.insert(qMakePair(100, QString("System")), edid);
    }
} EDIDFactoryOSX;
