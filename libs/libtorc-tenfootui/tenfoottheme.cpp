/* Class TenfootTheme
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
#include <QDomDocument>
#include <QThreadPool>

// Torc
#include "torclogging.h"
#include "torclocaldefs.h"
#include "tenfootthemeloader.h"
#include "tenfoottheme.h"

#define LOC QString("Tenfoot: ")

int TenfootTheme::kTenfootThemeType = UIWidget::RegisterWidgetType();

TenfootTheme::TenfootTheme(QSize WindowSize)
  : UITheme(WindowSize)
{
    m_type = kTenfootThemeType;
}

TenfootTheme::~TenfootTheme()
{
}

UITheme* TenfootTheme::Load(bool Immediate, UIWindow* Owner, const QString &FileName)
{
    QString themefile = FileName;

    if (themefile.isEmpty())
        themefile = UITheme::GetThemeFile(TORC_GUI_TENFOOT_THEMES);

    TenfootThemeLoader* loader = new TenfootThemeLoader(themefile, Owner, QThread::currentThread());
    if (Immediate)
        return loader->LoadTenfootTheme();

    QThreadPool::globalInstance()->start(loader, QThread::NormalPriority);
    return NULL;
}

bool TenfootTheme::ParseThemeFile(UITheme *Theme, UIWidget *Parent, QDomDocument *Document)
{
    if (!Document)
        return false;

    QDomElement root = Document->documentElement();

    for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling())
    {
        QDomElement element = node.toElement();
        if (element.isNull())
            continue;

        if (element.tagName() == "window")
        {
            if (!ParseWidget(Theme, Parent, &element))
                return false;
        }
        else if(element.tagName() == "font")
        {
            if (!ParseFont(Theme, &element))
                return false;
        }
    }

    return true;
}
