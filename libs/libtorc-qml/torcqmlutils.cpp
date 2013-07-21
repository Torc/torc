/* TorcQMLUtils
*
* This file is part of the Torc project.
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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Torc
#include "torclogging.h"
#include "torcqmlutils.h"

void TorcQMLUtils::AddProperty(const QString &Name, QObject* Property, QQmlContext *Context)
{
    if (Property && Context)
    {
        Context->setContextProperty(Name, Property);
        LOG(VB_GENERAL, LOG_INFO, QString("Added QML property '%1'").arg(Name));
    }
    else
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("Failed to add property %1").arg(Name));
    }
}
