/* Class UIDisplay
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012
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

#ifndef UIDISPLAY_H
#define UIDISPLAY_H

// Torc
#include "uidisplaybase.h"

class UIDisplay : public UIDisplayBase
{
  public:
    UIDisplay(QWidget *Widget);
    virtual ~UIDisplay();

    bool     InitialiseDisplay   (void);
    void     SwitchToMode        (int Index);

  protected:
    double   GetRefreshRatePriv  (void);
    QSize    GetPhysicalSizePriv (void);
};

#endif // UIDISPLAY_H
