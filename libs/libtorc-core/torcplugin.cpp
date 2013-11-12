/* Class TorcPlugin
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

// Qt
#include <QDirIterator>

// Torc
#include "torclogging.h"
#include "torcdirectories.h"
#include "torcplugin.h"

/*! \class TorcPlugin
 *  \brief A class to load plugins at run time.
 *
 * TorcPlugin will by default search /usr/local/lib/torc/plugins for libraries prefixed with 'libtorc'.
 * Additionaly, it will search the directory indicated by TORC_RUNTIME_LIBS, if provided. The latter
 * is useful for development and testing.
 *
 * \note Symbolic links are not followed.
 *
 * \todo Extend the LoadPlugin 'interface'
*/
TorcPlugin::TorcPlugin()
{
    // try the standard plugin directory first
    Load(GetTorcPluginDir());

    // and look at any user specified directory as well
    QString fallback = qgetenv("TORC_RUNTIME_LIBS");
    if (!fallback.isEmpty())
        Load(fallback);
}

///brief Unload previously loaded plugins
TorcPlugin::~TorcPlugin()
{
    LOG(VB_GENERAL, LOG_INFO, QString("Need to unload %1 plugins").arg(m_loadedPlugins.size()));

    while (!m_loadedPlugins.isEmpty())
    {
        TorcLibrary* library = m_loadedPlugins.takeLast();
        if (library->isLoaded())
        {
            // allow the plugin to cleanup
            TORC_UNLOAD_PLUGIN unload = (TORC_UNLOAD_PLUGIN)library->resolve("UnloadPlugin");
            if (unload)
                if (!unload())
                    LOG(VB_GENERAL, LOG_INFO, QString("UnloadPlugin call failed for '%1'").arg(library->fileName()));

            // actually unload it
            if (!library->unload())
                LOG(VB_GENERAL, LOG_WARNING, QString("Failed to unload plugin '%1'").arg(library->fileName()));
        }
        delete library;
    }
}

///brief Search the given directory for torc libraries and attempt to load and initialise them.
void TorcPlugin::Load(const QString &Directory)
{
    QStringList filter;
    filter.append("libtorc*");

    QDirIterator it(Directory, filter, QDir::NoDotAndDotDot | QDir::Files | QDir::Readable | QDir::Executable | QDir::NoSymLinks);

    if (!it.hasNext())
    {
        LOG(VB_GENERAL, LOG_INFO, QString("No plugins found in '%1'").arg(Directory));
        return;
    }

    while (it.hasNext())
    {
        it.next();
        TorcLibrary* library = new TorcLibrary(it.filePath());

        TORC_LOAD_PLUGIN     loadplugin = (TORC_LOAD_PLUGIN)library->resolve("LoadPlugin");
        TORC_UNLOAD_PLUGIN unloadplugin = (TORC_UNLOAD_PLUGIN)library->resolve("UnloadPlugin");
        bool unload = false;

        // fail if the plugin does not implement both LoadPlugin and UnloadPlugin
        if (!unloadplugin)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Plugin '%1' does not implement UnloadPlugin").arg(library->fileName()));
            unload = true;
        }
        else if (!loadplugin)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Plugin '%1' does not implement LoadPlugin").arg(library->fileName()));
            unload = true;
        }
        else
        {
            if (!loadplugin(QT_VERSION_STR))
            {
                LOG(VB_GENERAL, LOG_INFO, QString("Plugin '%1' initialisation failed. Disabling").arg(library->fileName()));
                unload = true;
            }
        }

        if (unload)
        {
            // NB unload() may not work on all platforms
            if (!library->unload())
                LOG(VB_GENERAL, LOG_ERR, QString("Failed to unload '%1'").arg(it.fileName()));
            delete library;
        }
        else
        {
            m_loadedPlugins.append(library);
        }
    }
}
