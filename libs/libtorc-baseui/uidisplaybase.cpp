/* Class UIDisplayBase
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012-13
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

#include <math.h>

#define VALID_RATE(rate) (rate > 20.0 && rate < 200.0)

static inline double FixRate(double Rate)
{
    static const double defaultrate = 60.0;
    if (VALID_RATE(Rate))
        return Rate;
    return defaultrate;
}

static inline bool IsStandardScreenRatio(double Ratio)
{
    static double ratios[] = { 4.0f / 3.0f, 16.0f / 9.0f, 16.0f / 10.0f };
    double high = Ratio + 0.01f;
    double low =  Ratio - 0.01f;

    for (uint i = 0; i < sizeof(ratios) / sizeof(double); ++i)
        if (low < ratios[i] && high > ratios[i])
            return true;

    return false;
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

UIDisplayMode::UIDisplayMode()
  : m_width(0),
    m_height(0),
    m_depth(0),
    m_rate(0.0f),
    m_interlaced(false),
    m_index(-1)
{
}

UIDisplayMode::UIDisplayMode(int Width, int Height, int Depth, double Rate, bool Interlaced, int Index)
  : m_width(Width),
    m_height(Height),
    m_depth(Depth),
    m_rate(Rate),
    m_interlaced(Interlaced),
    m_index(Index)
{
}

UIDisplayBase::UIDisplayBase(QWidget *Widget)
  : m_pixelSize(-1, -1),
    m_screen(0),
    m_screenCount(1),
    m_physicalSize(-1, -1),
    m_refreshRate(-1.0),
    m_originalRefreshRate(-1.0),
    m_originalModeIndex(-1),
    m_variableRefreshRate(false),
    m_aspectRatio(1.0f),
    m_pixelAspectRatio(1.0f),
    m_widget(Widget),
    m_lastRateChecked(-1.0f)
{
}

UIDisplayBase::~UIDisplayBase()
{
}

bool UIDisplayBase::CanHandleVideoRate(double Rate, int &ModeIndex)
{
    // can handle anything
    if (m_variableRefreshRate)
        return true;

    // avoid repeated checks
    if (qFuzzyCompare(Rate + 1.0f, m_lastRateChecked + 1.0f))
        return false;

    m_lastRateChecked = Rate;

    // TODO try and find a better rate when the current rate is below the video frame rate

    // Pick the most suitable rate from those available
    // 1. higher is better
    // 2. exact multiple is better
    // 3. progressive is better than interlaced (we can't guarantee tv interlacing will work)
    // 4. something in the range 50-60Hz is optimal for UI

    LOG(VB_GENERAL, LOG_INFO, QString("Trying to find best match for frame rate %1").arg(Rate));

    int best  = 0;
    int index = -1;
    QList<int> scores;
    for (int i = 0; i < m_modes.size(); ++i)
    {
        int    score = 0;
        double rate  = m_modes[i].m_rate;

        // optimum range
        if (rate > 49.0f && rate < 61.0f)
            score += 5;

        // less desirable
        if (m_modes[i].m_interlaced)
            score -= 3;

        // exact match
        if (qFuzzyCompare(Rate + 1.0f, rate + 1.0f))
        {
            score += 15;
        }
        // multiple
        else if (qFuzzyCompare((double)1.0f, fmod(rate, Rate) + 1.0f))
        {
            score += 15;
        }
        // try to account for 29.97/30.00 and 23.97/24.00 differences
        else if (abs(rate - Rate) < 0.05f)
        {
            score += 10;
        }
        else if (fmod(rate, Rate) < 0.01f)
        {
            score += 10;
        }

        // avoid dropping frames
        if (rate < (Rate - 0.05f))
            score -= 10;

        if (score > best)
        {
            best = score;
            index = i;
        }

        LOG(VB_GUI, LOG_INFO, QString("Rate: %1Hz%2 Score %3 Index %4")
            .arg(m_modes[i].m_rate).arg(m_modes[i].m_interlaced ? QString(" Interlaced") : "")
            .arg(score).arg(m_modes[i].m_index));
    }

    if (best < 1)
    {
        LOG(VB_GENERAL, LOG_INFO, "Failed to find suitable rate");
        return false;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Best mode %1Hz%2 (Index %3)")
        .arg(m_modes[index].m_rate).arg(m_modes[index].m_interlaced ? QString(" Interlaced") : "")
        .arg(m_modes[index].m_index));

    ModeIndex = index;
    return true;
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

double UIDisplayBase::GetDefaultRefreshRate(void)
{
    return m_originalRefreshRate;
}

int UIDisplayBase::GetDefaultMode(void)
{
    return m_originalModeIndex;
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

    if (!IsStandardScreenRatio(m_aspectRatio))
    {
        // try to fix aspect ratios that are based on innacurate EDID physical size data (EDID only provides
        // centimeter accuracy by default). Use the raw pixel aspect ratio as a hint.
        double rawpar = (double)m_pixelSize.width() / (double)m_pixelSize.height();
        if (IsStandardScreenRatio(rawpar) && (abs(m_aspectRatio - rawpar) < 0.1))
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Adjusting screen aspect ratio from %1 to %2").arg(m_aspectRatio).arg(rawpar));
            m_aspectRatio = rawpar;
        }
    }

    m_pixelAspectRatio = FixRatio(((double)m_pixelSize.width() / (double)m_pixelSize.height()) / m_aspectRatio);

    LOG(VB_GENERAL, LOG_INFO, QString("Using screen %1 of %2")
        .arg(m_screen + 1).arg(m_screenCount));
    LOG(VB_GENERAL, LOG_INFO, QString("Screen size : %1mm x %2mm (aspect %3)")
        .arg(m_physicalSize.width()).arg(m_physicalSize.height()).arg(m_aspectRatio));
    LOG(VB_GENERAL, LOG_INFO, QString("Screen size : %1px x %2px")
        .arg(m_pixelSize.width()).arg(m_pixelSize.height()));
    LOG(VB_GENERAL, LOG_INFO, QString("Refresh rate: %1Hz").arg(m_refreshRate));

    if (m_variableRefreshRate)
    {
        LOG(VB_GENERAL, LOG_INFO, "Available rate: Any");
    }
    else
    {
        QList<UIDisplayMode>::iterator it = m_modes.begin();
        for ( ; it != m_modes.end(); ++it)
            LOG(VB_GENERAL, LOG_INFO, QString("Available rate: %1Hz%2").arg((*it).m_rate).arg((*it).m_interlaced ? QString(" Interlaced") : ""));
    }

    if (m_originalRefreshRate < 0.0f)
        m_originalRefreshRate = m_refreshRate;
}
