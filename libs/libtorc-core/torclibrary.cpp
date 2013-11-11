/* TorcLibrary
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
* You should have received a copy of the GNU General Public License
*
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Torc
#include "torclogging.h"
#include "torccommandline.h"
#include "torclibrary.h"

// register use of fallback directory
static bool registered = TorcCommandLine::RegisterEnvironmentVariable("TORC_RUNTIME_LIBS", "Search for dynamically loaded libraries in this directory.");

/*! \class TorcLibrary
 *  \brief A convenience wrapper around QLibrary
 *
 * TorcLibrary will attemp to dynamically load a library in a platform independent manner. If the library cannot
 * be loaded using the current system paths, it will attempt to fallback to a user defined directory that can be set
 * with the environment variable TORC_RUNTIME_LIBS.
*/

///brief Load the given library and require the given Version.
TorcLibrary::TorcLibrary(const QString &FileName, int Version)
  : QLibrary(FileName, Version)
{
    Load();
}

///brief Load the given library using any available version.
TorcLibrary::TorcLibrary(const QString &FileName)
  : QLibrary(FileName)
{
    Load();
}

TorcLibrary::~TorcLibrary()
{
}

void TorcLibrary::Load(void)
{
    if (!load())
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("Failed to load '%1' using system paths (Error: '%2')").arg(fileName()).arg(errorString()));

        // try the fallback
        QString dir = qgetenv("TORC_RUNTIME_LIBS");

        if (!dir.isEmpty())
        {
            if (!dir.endsWith("/"))
                dir.append("/");

            LOG(VB_GENERAL, LOG_INFO, QString("Trying user defined directory: '%1'").arg(dir));
            setFileName(dir + fileName());

            if (!load())
                LOG(VB_GENERAL, LOG_WARNING, QString("Failed to load '%1' from fallback directory (Error: '%2')").arg(fileName()).arg(errorString()));
        }
    }

    if (isLoaded())
        LOG(VB_GENERAL, LOG_INFO, QString("Loaded '%1'").arg(fileName()));
}
