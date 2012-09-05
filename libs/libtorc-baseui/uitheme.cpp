/* Class UITheme
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
#include <QThreadPool>
#include <QDomDocument>
#include <QFontMetrics>
#include <QUrl>
#include <QDir>

// Torc
#include "torcdirectories.h"
#include "torclocalcontext.h"
#include "torclogging.h"
#include "uithemeloader.h"
#include "uitheme.h"
#include "uifont.h"

int UITheme::kUIThemeType = UIWidget::RegisterWidgetType();

UITheme::UITheme(QSize WindowSize)
  : UIWidget(this, NULL, "ThemeRoot", WidgetFlagNone),
    m_state(ThemeNull),
    m_size(0, 0),
    m_aspectRatio(0.0),
    m_versionMajor(0),
    m_versionMinor(0),
    m_windowSize(QSizeF(WindowSize))
{
    m_type = kUIThemeType;
}

UITheme::~UITheme()
{
}

bool UITheme::ValidateThemeInfo(void)
{
    // set the scaling factors
    m_scaleX = (qreal)m_windowSize.width()  / m_size.width();
    m_scaleY = (qreal)m_windowSize.height() / m_size.height();

    LOG(VB_GENERAL, LOG_DEBUG, QString("Description : %1").arg(m_description));
    LOG(VB_GENERAL, LOG_DEBUG, QString("Author name : %1").arg(m_authorName));
    LOG(VB_GENERAL, LOG_DEBUG, QString("Author email: %1").arg(m_authorEmail));
    LOG(VB_GENERAL, LOG_DEBUG, QString("Root window : %1").arg(m_rootWindow));
    LOG(VB_GENERAL, LOG_DEBUG, QString("Preview file: %1").arg(m_previewImage));
    LOG(VB_GENERAL, LOG_DEBUG, QString("Version     : %1.%2").arg(m_versionMajor).arg(m_versionMinor));
    LOG(VB_GENERAL, LOG_DEBUG, QString("Resolution  : %1x%2").arg(m_size.width()).arg(m_size.height()));
    LOG(VB_GENERAL, LOG_DEBUG, QString("Aspect ratio: %1").arg(m_aspectRatio));
    LOG(VB_GENERAL, LOG_DEBUG, QString("Scaling     : %1x%2").arg(m_scaleX).arg(m_scaleY));
    LOG(VB_GENERAL, LOG_DEBUG, QString("Window      : %1x%2").arg(m_windowSize.width()).arg(m_windowSize.height()));

    foreach (QString global, m_globals)
        LOG(VB_GENERAL, LOG_DEBUG, QString("Global      : %1").arg(global));

    if (!m_name.isEmpty() &&
        !m_description.isEmpty() &&
        !m_authorName.isEmpty() &&
        !m_authorEmail.isEmpty() &&
        !m_rootWindow.isEmpty() &&
        !m_previewImage.isEmpty() &&
        !m_size.isEmpty() &&
        m_aspectRatio > 0.0 && m_aspectRatio < 10.0)
    {
        SetState(UITheme::ThemeInfoOnly);
        LOG(VB_GENERAL, LOG_INFO, QString("Loaded theme info for '%1'")
            .arg(m_name));
        return true;
    }

    SetState(UITheme::ThemeError);
    LOG(VB_GENERAL, LOG_ERR, "Failed to load theme info.");
    return false;
}

void UITheme::ActivateTheme(void)
{
    if (GetState() != UITheme::ThemeReady)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot activate theme that isn't ready.");
        return;
    }

    // disable all top level widgets
    foreach (UIWidget* child, m_children)
        child->SetEnabled(false);

    // finalise the theme
    Finalise();

    // Find the default startup widget and enable it
    UIWidget* root = FindChildByName(m_rootWindow);
    if (root)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Activating root widget '%1'").arg(m_rootWindow));
        root->Activate();
        return;
    }

    LOG(VB_GENERAL, LOG_ERR, QString("Failed to find root widget '%1'").arg(m_rootWindow));
}

void UITheme::SetState(ThemeState State)
{
    m_state = State;
}

UITheme::ThemeState UITheme::GetState(void)
{
    return m_state;
}

QString UITheme::GetDirectory(void)
{
    return m_directory;
}

QString UITheme::GetThemeFile(const QString &ThemeType, const QString &Override)
{
    QString root  = GetTorcShareDir() + ThemeType + "/";
    QString theme = gLocalContext->GetSetting(TORC_GUI + ThemeType + ".theme", QString("default"));
    QString dir   = root + theme;
    QString file  = dir + "/theme.xml";

    bool valid = true;

    // check the directory exists
    QDir directory(dir);
    if (!directory.exists())
    {
        LOG(VB_GENERAL, LOG_ERR, "Directory '%1'' does not exist.");
        valid = false;
    }

    // find 'theme.xml'
    if (!QFile::exists(file))
    {
        LOG(VB_GENERAL, LOG_ERR, "File '%1'' does not exist.");
        valid = false;
    }

    if (valid)
        return file;

    // try and fallback to the default
    if (theme != "default" && Override.isEmpty())
        return GetThemeFile(ThemeType, "default");

    // error
    LOG(VB_GENERAL, LOG_ERR, QString("Failed to find a theme file (type '%1')")
        .arg(ThemeType));
    return QString("");
}

UITheme* UITheme::Load(const QString &Filename)
{
    UITheme* theme = new UITheme();
    theme->SetState(UITheme::ThemeLoadingInfo);
    UIThemeLoader* loader = new UIThemeLoader(Filename, theme);
    QThreadPool::globalInstance()->start(loader, QThread::NormalPriority);
    return theme;
}

bool UITheme::ParseThemeInfo(UITheme* Theme, QDomDocument *Document)
{
    if (!Document)
        return false;

    QDomElement root = Document->documentElement();

    for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling())
    {
        QDomElement element = node.toElement();
        if (element.isNull())
            continue;

        QString tagname = element.tagName();
        if (tagname == "name")
        {
            Theme->m_name = GetText(&element);
        }
        else if (tagname == "description")
        {
            Theme->m_description = GetText(&element);
        }
        else if (tagname == "preview")
        {
            Theme->m_previewImage = GetText(&element);
        }
        else if (tagname == "rootwindow")
        {
            Theme->m_rootWindow = GetText(&element);
        }
        else if (tagname == "resolution")
        {
            QString size = GetText(&element);
            Theme->m_size = QSizeF(size.section('x', 0, 0).toFloat(),
                                   size.section('x', 1, 1).toFloat());
        }
        else if (tagname == "aspectratio")
        {
            QString aspect = GetText(&element);
            qreal numerator   = aspect.section(":", 0, 0).toFloat();
            qreal denominator = aspect.section(":", 1, 1).toFloat();
            if (numerator > 0.0 && denominator > 0.0)
                Theme->m_aspectRatio = numerator / denominator;
        }
        else if (tagname == "author")
        {
            for (QDomNode child = element.firstChild(); !child.isNull();
                 child = child.nextSibling())
            {
                QDomElement childelement = child.toElement();
                if (!childelement.isNull())
                {
                    if (childelement.tagName() == "name")
                        Theme->m_authorName = GetText(&childelement);
                    else if (childelement.tagName() == "email")
                        Theme->m_authorEmail = GetText(&childelement);
                }
            }
        }
        else if (tagname == "version")
        {
            for (QDomNode child = element.firstChild(); !child.isNull();
                 child = child.nextSibling())
            {
                QDomElement childelement = child.toElement();
                if (!childelement.isNull())
                {
                    if (childelement.tagName() == "major")
                        Theme->m_versionMajor = GetText(&childelement).toUInt();
                    else if (childelement.tagName() == "minor")
                        Theme->m_versionMinor = GetText(&childelement).toUInt();
                }
            }
        }
        else if (tagname == "globals")
        {
            for (QDomNode child = element.firstChild(); !child.isNull();
                 child = child.nextSibling())
            {
                QDomElement childelement = child.toElement();
                if (!childelement.isNull())
                    if (childelement.tagName() == "file")
                        Theme->m_globals.append(GetText(&childelement));
            }
        }
    }

    if (Theme->ValidateThemeInfo())
    {
        Theme->SetState(UITheme::ThemeInfoOnly);
        return true;
    }

    return false;
}
