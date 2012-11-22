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
#include "torclogging.h"
#include "uidisplaybase.h"

#define VALID_RATE(rate) (rate > 20.0 && rate < 200.0)

static inline double FixRate(double Rate)
{
    static const double defaultrate = 120.0;
    if (VALID_RATE(Rate))
        return Rate;
    return defaultrate;
}

static inline double FixRatio(double Ratio)
{
    static double ratios[] = { 1.0f, 4.0f / 3.0f, 16.0f / 9.0f, 16.0f / 10.0f, 5.0f / 4.0f, 2.0f / 1.0f, 3.0f / 2.0f, 14.0f / 9.0f, 21.0f / 10.0f };
    double high = Ratio + 0.01f;
    double low =  Ratio - 0.01f;

    for (uint i = 0; i < sizeof(ratios) / sizeof(double); ++i)
        if (low < ratios[i] && high > ratios[i])
            return ratios[i];

    return Ratio;
}

UIDisplayBase::UIDisplayBase(QWidget *Widget)
  : m_pixelSize(-1, -1),
    m_screen(0),
    m_screenCount(1),
    m_physicalSize(-1, -1),
    m_refreshRate(-1.0),
    m_aspectRatio(1.0f),
    m_pixelAspectRatio(1.0f),
    m_widget(Widget)
{
}

UIDisplayBase::~UIDisplayBase()
{
}

int UIDisplayBase::GetScreen(void)
{
    return m_screen;
}

int UIDisplayBase::GetScreenCount(void)
{
    return m_screenCount;
}

QSize UIDisplayBase::GetGeometry(void)
{
    return m_pixelSize;
}

double UIDisplayBase::GetRefreshRate(void)
{
    return m_refreshRate;
}

QSize UIDisplayBase::GetPhysicalSize(void)
{
    return m_physicalSize;
}

double UIDisplayBase::GetDisplayAspectRatio(void)
{
    return m_aspectRatio;
}

double UIDisplayBase::GetPixelAspectRatio(void)
{
    return m_pixelAspectRatio;
}

int UIDisplayBase::GetScreenPriv(void)
{
    return QApplication::desktop()->screenNumber(m_widget);
}

int UIDisplayBase::GetScreenCountPriv(void)
{
    return QApplication::desktop()->screenCount();
}

QSize UIDisplayBase::GetGeometryPriv(void)
{
    return QApplication::desktop()->screenGeometry(GetScreen()).size();
}

void UIDisplayBase::Sanitise(void)
{
    m_refreshRate      = FixRate(m_refreshRate);
    m_aspectRatio      = FixRatio((double)m_physicalSize.width() / (double)m_physicalSize.height());
    m_pixelAspectRatio = FixRatio(((double)m_physicalSize.height() / (double)m_pixelSize.height()) /
                                  ((double)m_physicalSize.width() / (double)m_pixelSize.width()));


    LOG(VB_GENERAL, LOG_INFO, QString("Using screen %1 of %2")
        .arg(m_screen + 1).arg(m_screenCount));
    LOG(VB_GENERAL, LOG_INFO, QString("Refresh rate: %1Hz").arg(m_refreshRate));
    LOG(VB_GENERAL, LOG_INFO, QString("Screen size : %1mm x %2mm (aspect %3)")
        .arg(m_physicalSize.width()).arg(m_physicalSize.height()).arg(m_aspectRatio));
    LOG(VB_GENERAL, LOG_INFO, QString("Screen size : %1px x %2px")
        .arg(m_pixelSize.width()).arg(m_pixelSize.height()));
}
