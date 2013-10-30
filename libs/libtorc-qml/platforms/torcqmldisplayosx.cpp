/* Class TorcQMLDisplayOSX
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

// Torc
#include "torclogging.h"
#include "torccocoa.h"
#include "torcedid.h"
#include "torcqmldisplayosx.h"

CGDirectDisplayID GetOSXDisplay(WId win);
int               DepthFromStringRef(CFStringRef Format);

CGDirectDisplayID GetOSXDisplay(WId win)
{
    if (!win)
        return 0;

    return GetOSXCocoaDisplay((void*)win);
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

TorcQMLDisplayOSX::TorcQMLDisplayOSX(QWindow *Window)
  : TorcQMLDisplay(Window),
    m_displayModes(NULL)
{
}

TorcQMLDisplayOSX::~TorcQMLDisplayOSX()
{
    if (m_displayModes)
        CFRelease(m_displayModes);
    m_displayModes = NULL;
}

void TorcQMLDisplayOSX::UpdateScreenData(void)
{
    if (!m_window || !m_screen)
        return;

    TorcQMLDisplay::UpdateScreenData();
    RefreshScreenModes();
    m_edid = TorcEDID::GetEDID(m_window, screenNumber);
    setScreenCECPhysicalAddress(m_edid.PhysicalAddress());
    emit screenCECPhysicalAddressChanged(screenCECPhysicalAddress);
}

void TorcQMLDisplayOSX::RefreshScreenModes(void)
{
    if (!m_window)
        return;

    // release existing modes
    if (m_displayModes)
        CFRelease(m_displayModes);
    m_displayModes = NULL;

    // retrieve new list of modes
    CGDirectDisplayID display = GetOSXDisplay(m_window->winId());
    if (!display)
        return;

    CGDisplayModeRef current = CGDisplayCopyDisplayMode(display);
    m_displayModes = CGDisplayCopyAllDisplayModes(display, NULL);

    if (m_displayModes)
    {
        for (int i = 0; i < CFArrayGetCount(m_displayModes); ++i)
        {
            CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(m_displayModes, i);
            int modewidth         = CGDisplayModeGetWidth(mode);
            int modeheight        = CGDisplayModeGetHeight(mode);
            double moderate       = CGDisplayModeGetRefreshRate(mode);
            // internal OSX displays have 'flexible' refresh rates, with a max of 60Hz - but report 0hz
            if (moderate < 0.1f)
                moderate = 60.0f;
            int32_t flags         = CGDisplayModeGetIOFlags(mode);
            bool interlaced       = flags & kDisplayModeInterlacedFlag;
            CFStringRef fmt       = CGDisplayModeCopyPixelEncoding(mode);
            int depth             = DepthFromStringRef(fmt);
            CFRelease(fmt);

            bool ignore = modewidth  != screenPixelSize.width() ||
                          modeheight != screenPixelSize.height() ||
                          (flags & kDisplayModeNotGraphicsQualityFlag) ||
                         !(flags & kDisplayModeSafetyFlags) ||
                          depth < 32 || moderate < 10.0f || moderate > 121.0f;

            LOG(VB_GENERAL, LOG_INFO, QString("Mode %1x%2@%3Hz %4bpp%5%6%7")
                .arg(modewidth).arg(modeheight).arg(moderate).arg(depth)
                .arg(interlaced ? QString(" Interlaced") : "")
                .arg(ignore ? QString(" Ignoring") : "")
                .arg(mode == current ? QString(" CURRENT"): ""));

            if (!ignore)
                m_modes.append(TorcDisplayMode(modewidth, modeheight, depth, moderate, interlaced, i));
        }
    }
}

class TorcEDIDFactoryOSX : public TorcEDIDFactory
{
    void GetEDID(QMap<QPair<int, QString>, QByteArray> &EDIDMap, QWindow *Window, int Screen)
    {
        (void)Screen;

        CocoaAutoReleasePool pool;
        CGDirectDisplayID disp = GetOSXDisplay(Window->winId());
        QByteArray edid = GetOSXEDID(disp);
        if (!edid.isEmpty())
            EDIDMap.insert(qMakePair(100, QString("System")), edid);
    }
} TorcEDIDFactoryOSX;

class TorcQMLDisplayFactoryOSX : public TorcQMLDisplayFactory
{
    void ScoreTorcQMLDisplay(QWindow *Window, int &Score)
    {
        if (Score < 20)
            Score = 20;
    }

    TorcQMLDisplay* GetTorcQMLDisplay(QWindow *Window, int Score)
    {
        if (Score == 20)
            return new TorcQMLDisplayOSX(Window);
        return NULL;
    }
} TorcQMLDisplayFactoryOSX;
