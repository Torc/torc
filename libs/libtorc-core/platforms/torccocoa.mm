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
#import <IOKit/graphics/IOGraphicsLib.h>

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

    NSDictionary* dict = (NSDictionary*)IODisplayCreateInfoDictionary(displayport, kIODisplayOnlyPreferredName);

    NSData* edid = [dict objectForKey: [NSString stringWithUTF8String: kIODisplayEDIDKey]];
    if (edid && [edid isKindOfClass: [NSData class]] && [edid length] != 0)
        result = QByteArray((char*)[edid bytes], [edid length]);

    return result;
}
