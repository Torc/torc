/* Class UIThemeLoader
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
#include <QUrl>
#include <QFile>
#include <QDomDocument>
#include <QScopedPointer>

// Torc
#include "torclogging.h"
#include "torcbuffer.h"
#include "uitheme.h"
#include "uithemeloader.h"

#define LOC QString("UILoader: ")

UIThemeLoader::UIThemeLoader(const QString &FileName, UITheme *Theme)
  : m_filename(FileName),
    m_theme(Theme)
{
    if (m_theme)
        m_theme->UpRef();
}

UIThemeLoader::~UIThemeLoader()
{
    if (m_theme)
        m_theme->DownRef();
    m_theme = NULL;
}

bool UIThemeLoader::LoadThemeInfo(void)
{
    // sanity check
    if (!m_theme || m_filename.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid theme data.");
        m_theme->SetState(UITheme::ThemeError);
        return false;
    }

    // open the info file
    int abort = 0;
    QScopedPointer<TorcBuffer> file(TorcBuffer::Create(m_filename, &abort));
    if (!file.data())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to open '%1'")
            .arg(m_filename));
        m_theme->SetState(UITheme::ThemeError);
        return false;
    }

    // parse it
    QByteArray   content(file.data()->ReadAll(20000/*20 seconds*/));
    QDomDocument document;
    QString      error;
    int          errorline = 0;
    int          errorcolumn = 0;

    if (!document.setContent(content, false, &error, &errorline, &errorcolumn))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to parse '%1'. Error '%2', line %3, column %4")
            .arg(m_filename).arg(error).arg(errorline).arg(errorcolumn));
        m_theme->SetState(UITheme::ThemeError);
        return false;
    }

    // extract the theme info
    return UITheme::ParseThemeInfo(m_theme, &document);
}

void UIThemeLoader::run()
{
    LoadThemeInfo();
}
