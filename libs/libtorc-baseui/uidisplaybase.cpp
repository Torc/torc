/* Class UIDisplayBase
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

// Qt
#include <QApplication>
#include <QDesktopWidget>

// Torc
#include "uidisplaybase.h"

UIDisplayBase::UIDisplayBase(QWidget *Widget)
  : m_pixelSize(-1, -1),
    m_screen(0),
    m_screenCount(1),
    m_widget(Widget)
{
}

UIDisplayBase::~UIDisplayBase()
{
}

int UIDisplayBase::GetScreen(void)
{
    return QApplication::desktop()->screenNumber(m_widget);
}

int UIDisplayBase::GetScreenCount(void)
{
    return QApplication::desktop()->screenCount();
}

QSize UIDisplayBase::GetGeometry(void)
{
    return QApplication::desktop()->screenGeometry(GetScreen()).size();
}
