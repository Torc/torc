/* Class TenfootThemeLoader
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
#include <QThread>
#include <QDomDocument>
#include <QScopedPointer>

// Torc
#include "torclogging.h"
#include "torcbuffer.h"
#include "uitheme.h"
#include "uiwindow.h"
#include "tenfoottheme.h"
#include "tenfootthemeloader.h"

#define LOC QString("UILoader: ")

TenfootThemeLoader::TenfootThemeLoader(const QString &FileName, UIWindow *Owner, QThread *OwnerThread)
  : UIThemeLoader(FileName, NULL),
    m_owner(Owner),
    m_ownerThread(OwnerThread)
{
}

TenfootThemeLoader::~TenfootThemeLoader()
{
}

UITheme* TenfootThemeLoader::LoadTenfootTheme(void)
{
    if (!m_owner || !m_ownerThread)
    {
        LOG(VB_GENERAL, LOG_ERR, "No owner for theme.");
        return NULL;
    }

    m_theme = new TenfootTheme(m_owner->GetSize());
    m_theme->SetState(UITheme::ThemeLoadingInfo);

    if (!LoadThemeInfo())
    {
        m_theme->DownRef();
        return NULL;
    }

    m_theme->SetState(UITheme::ThemeLoadingFull);

    int abort = 0;

    QScopedPointer<TorcBuffer> file(TorcBuffer::Create(m_filename, &abort));
    if (!file.data())
    {
        m_theme->DownRef();
        return NULL;
    }

    m_theme->m_directory = file.data()->GetPath();

    // parse each global if requested
    foreach (QString global, m_theme->m_globals)
    {
        global = m_theme->m_directory + "/" + global;

        // xml only for now
        if (!global.toLower().endsWith("xml"))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "XML files only for now.");
            m_theme->DownRef();
            return NULL;
        }

        // open
        QScopedPointer<TorcBuffer> globalfile(TorcBuffer::Create(global, &abort));
        if (!globalfile.data())
        {
            m_theme->DownRef();
            return NULL;
        }

        // parse it
        QByteArray   content(globalfile.data()->ReadAll(20000/*20 seconds*/));
        QDomDocument document;
        QString      error;
        int          errorline = 0;
        int          errorcolumn = 0;

        if (!document.setContent(content, false, &error, &errorline, &errorcolumn))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Failed to parse '%1'. Error '%2', line %3, column %4")
                .arg(global).arg(error).arg(errorline).arg(errorcolumn));
            m_theme->DownRef();
            return NULL;
        }

        // extract window information
        if (!TenfootTheme::ParseThemeFile(m_theme, m_theme, &document))
        {
            m_theme->DownRef();
            return NULL;
        }
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Finished loading theme '%1'")
        .arg(m_theme->m_name));

    m_theme->UpRef();
    m_theme->Debug();
    m_theme->SetState(UITheme::ThemeReady);

    return m_theme;
}

void TenfootThemeLoader::run()
{
    UITheme* theme = LoadTenfootTheme();
    if (theme)
    {
        theme->moveToThread(m_ownerThread);
        m_owner->ThemeReady(theme);
    }
}
