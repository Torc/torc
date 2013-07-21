/* Class TorcQMLEventProxy
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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

// Torc
#include "torclocalcontext.h"
#include "torcevent.h"
#include "torcqmleventproxy.h"

TorcQMLEventProxy::TorcQMLEventProxy(QWindow *Window)
  : QObject(),
    m_window(Window)
{
    if (m_window)
        gLocalContext->AddObserver(this);
}

TorcQMLEventProxy::~TorcQMLEventProxy()
{
    if (m_window)
        gLocalContext->RemoveObserver(this);
}

bool TorcQMLEventProxy::event(QEvent *Event)
{
    if (Event->type() == TorcEvent::TorcEventType)
    {
        TorcEvent* event = static_cast<TorcEvent*>(Event);
        if (event && event->GetEvent() == Torc::Exit && m_window)
        {
            LOG(VB_GENERAL, LOG_INFO, "Closing main window");
            m_window->close();
            return true;
        }
    }

    return false;
}
