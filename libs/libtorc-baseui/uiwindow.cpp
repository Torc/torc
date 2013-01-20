/* Class UIWindow
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
#include <QMutex>

// Torc
#include "torclocalcontext.h"
#include "torclogging.h"
#include "uitheme.h"
#include "uiwindow.h"

#ifdef Q_OS_WIN
#include "direct3d/uidirect3d9window.h"
#else
#include "opengl/uiopenglwindow.h"
#endif

UIWindow::UIWindow()
  : m_theme(NULL),
    m_newTheme(NULL),
    m_haveNewTheme(0),
    m_newThemeLock(new QMutex()),
    m_mainTimer(0)
{
    m_studioLevels = gLocalContext->GetSetting(TORC_GUI + "StudioLevels", (bool)false);
}

UIWindow::~UIWindow()
{
    if (m_theme)
        m_theme->DownRef();
    m_theme = NULL;

    if (m_newTheme)
        m_newTheme->DownRef();
    m_newTheme = NULL;

    delete m_newThemeLock;
    m_newThemeLock = NULL;
}

UIWindow* UIWindow::Create(void)
{
#ifdef Q_OS_WIN
    return UIDirect3D9Window::Create();
#else
    return UIOpenGLWindow::Create();
#endif
    return NULL;
}

void UIWindow::ThemeReady(UITheme *Theme)
{
    if (!Theme)
        return;

    {
        QMutexLocker locker(m_newThemeLock);

        if (Theme->GetState() != UITheme::ThemeReady)
        {
            Theme->DownRef();
            LOG(VB_GENERAL, LOG_ERR, "New theme is invalid.");
            return;
        }

        if (m_newTheme)
        {
            LOG(VB_GENERAL, LOG_WARNING, "Already have 1 new theme queued!!");
            m_newTheme->DownRef();
        }

        m_newTheme = Theme;
    }

    m_haveNewTheme.ref();
}

bool UIWindow::GetStudioLevels(void)
{
    return m_studioLevels;
}

void UIWindow::SetStudioLevels(bool Value)
{
    m_studioLevels = Value;
    gLocalContext->SetSetting(TORC_GUI + "StudioLevels", m_studioLevels);
}

void UIWindow::CheckForNewTheme(void)
{
    if (m_haveNewTheme.fetchAndAddOrdered(0) < 1)
        return;

    {
        QMutexLocker locker(m_newThemeLock);

        if (m_theme)
            m_theme->DownRef();

        m_theme = m_newTheme;
        m_newTheme = NULL;
        m_theme->ActivateTheme();
    }

    while (m_haveNewTheme.deref()) { }
}
