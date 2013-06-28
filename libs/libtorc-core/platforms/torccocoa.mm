/* Class TorcCocoa
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

// OS X
#import <Cocoa/Cocoa.h>

@implementation NSThread (Dummy)
- (void) run;
{
}
@end

/*! /class CocoaAutoReleasePool
 *  /brief A convenience class to instantiate a Cocoa auto release pool for a thread.
*/
CocoaAutoReleasePool::CocoaAutoReleasePool()
{
    // Inform the Cocoa framework that we are multithreaded so that it creates
    // its internal locks.
    if (![NSThread isMultiThreaded])
    {
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
        NSThread *thread = [[NSThread alloc] init];
        SEL threadSelector = @selector(run);
        [NSThread detachNewThreadSelector:threadSelector toTarget:thread withObject:nil];
        [pool release];
    }

    NSAutoreleasePool *pool = NULL;
    pool = [[NSAutoreleasePool alloc] init];
    m_pool = pool;
}

CocoaAutoReleasePool::~CocoaAutoReleasePool()
{
    if (m_pool)
    {
        NSAutoreleasePool *pool = (NSAutoreleasePool*) m_pool;
        [pool release];
    }
}

/// \brief Retrieve the display ID for the given window.
CGDirectDisplayID GetOSXCocoaDisplay(void* Window)
{
    NSView *thisview = static_cast<NSView *>(Window);
    if (!thisview)
        return 0;
    NSScreen *screen = [[thisview window] screen];
    if (!screen)
        return 0;
    NSDictionary* desc = [screen deviceDescription];
    return (CGDirectDisplayID)[[desc objectForKey:@"NSScreenNumber"] intValue];
}

/// \brief Get the EDID for the display Display.
QByteArray GetOSXEDID(CGDirectDisplayID Display)
{
    QByteArray result;
    if (!Display)
        return result;

    io_registry_entry_t displayport = CGDisplayIOServicePort(Display);

    CFDataRef edid = (CFDataRef)IORegistryEntrySearchCFProperty(displayport, kIOServicePlane, CFSTR("EDID"),
                                                                kCFAllocatorDefault,
                                                                kIORegistryIterateRecursively | kIORegistryIterateParents);

    const char* buf = (const char*)[(NSData*)edid bytes];

    if (!buf)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Failed to retrieve EDID for display - "
                                     "try rebooting if this is a hotplugged display");
        return result;
    }

    int length = [(NSData*)edid length];
    result = QByteArray(buf, length);

    return result;
}

/// \brief Get the EDID for the current display.
QByteArray GetOSXEDID(void)
{
    QByteArray result;

    // pool
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    // get displays
    CGDisplayCount count = 0;
    CGError err = CGGetActiveDisplayList(0, NULL, &count);

    if (err == CGDisplayNoErr)
    {
        CGDirectDisplayID *displays = (CGDirectDisplayID*)calloc((size_t)count, sizeof(CGDirectDisplayID));
        err = CGGetActiveDisplayList(count, displays, &count);

        if (err == CGDisplayNoErr)
        {
            for (uint i = 0; i < count; i++)
            {
                QByteArray ediddata = GetOSXEDID(displays[i]);

                if (!ediddata.isEmpty())
                {
                    result = ediddata;
                    break;
                }
            }
        }
    }

    [pool release];

    return result;
}
