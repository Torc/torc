/* TorcDirectories
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
#include <QDir>

// Torc
#include "torclogging.h"
#include "torcdirectories.h"

static QString gInstallDir = QString("");
static QString gShareDir   = QString("");
static QString gConfDir    = QString("");

/*! \brief Statically initialise the various directories that Torc uses.
 *
 * gInstallDir will default to /usr/local
 * gShareDir will default to  /usr/local/share
 * gConfDir will default to ~/.torc
 *
 * \sa GetTorcConfigDir
 * \sa GetTorcShareDir
*/
void InitialiseTorcDirectories(void)
{
    static bool initialised = false;
    if (initialised)
        return;

    initialised = true;

    gInstallDir = QString(PREFIX) + "/";
    gShareDir   = QString(RUNPREFIX) + "/";
    gConfDir    = QDir::homePath() + "/.torc";
}

/*! \brief Return the path to the application configuration directory
 *
 * This is the directory where (under a default setup) the database is stored.
*/
QString GetTorcConfigDir(void)
{
    return gConfDir;
}

/*! \brief Return the path to the installed Torc shared resources.
 *
 * Shared resources might include UI themes, other images and static HTML content.
*/
QString GetTorcShareDir(void)
{
    return gShareDir;
}
