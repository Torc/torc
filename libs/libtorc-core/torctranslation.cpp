/* Class TorcTranslation
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

// Torc
#include "torctranslation.h"

TorcStringFactory* TorcStringFactory::gTorcStringFactory = NULL;

TorcStringFactory::TorcStringFactory()
{
    nextTorcStringFactory = gTorcStringFactory;
    gTorcStringFactory = this;
}

TorcStringFactory::~TorcStringFactory()
{
}

TorcStringFactory* TorcStringFactory::GetTorcStringFactory(void)
{
    return gTorcStringFactory;
}

TorcStringFactory* TorcStringFactory::NextFactory(void) const
{
    return nextTorcStringFactory;
}

QMap<QString,QString> TorcStringFactory::GetTorcStrings(void)
{
    QMap<QString, QString> strings;
    TorcStringFactory* factory = TorcStringFactory::GetTorcStringFactory();
    for ( ; factory; factory = factory->NextFactory())
        factory->GetStrings(strings);
    return strings;
}


