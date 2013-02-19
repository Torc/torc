/* Class TorcOMXCore
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
#include "torcomxcore.h"

TorcOMXCore::TorcOMXCore(const QString &Library)
  : QLibrary(Library),
    m_initialised(false)
{
    m_omxInit                = (TORC_OMXINIT)               resolve("OMX_Init");
    m_omxDeinit              = (TORC_OMXDEINIT)             resolve("OMX_Deinit");
    m_omxComponentNameEnum   = (TORC_OMXCOMPONENTNAMEENUM)  resolve("OMX_ComponentNameEnum");
    m_omxGetHandle           = (TORC_OMXGETHANDLE)          resolve("OMX_GetHandle");
    m_omxFreeHandle          = (TORC_OMXFREEHANDLE)         resolve("OMX_FreeHandle");
    m_omxSetupTunnel         = (TORC_OMXSETUPTUNNEL)        resolve("OMX_SetupTunnel");
    m_omxGetComponentsOfRole = (TORC_OMXGETCOMPONENTSOFROLE)resolve("OMX_GetComponentsOfRole");
    m_omxGetRolesOfComponent = (TORC_OMXGETROLESOFCOMPONENT)resolve("OMX_GetRolesOfComponent");

    if (m_omxInit)
    {
        m_initialised = m_omxInit() == OMX_ErrorNone;

        if (!m_initialised)
            LOG(VB_GENERAL, LOG_ERR, "Failed to initialise OMXCore");
    }

    if (IsValid())
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Loaded OpenMaxIL library '%1'").arg(Library));
        LOG(VB_GENERAL, LOG_INFO, "Available components:");
        OMX_U32 index = 0;
        char componentname[128];
        while (m_omxComponentNameEnum(&componentname[0], 128, index++) == OMX_ErrorNone)
            LOG(VB_GENERAL, LOG_INFO, componentname);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to load OpenMax library '%1'").arg(Library));
    }
}

TorcOMXCore::~TorcOMXCore()
{
    if (IsValid())
    {
        LOG(VB_GENERAL, LOG_INFO, "Closing OpenMax Core");
        m_omxDeinit();
    }
}

bool TorcOMXCore::IsValid(void)
{
    return isLoaded() && m_initialised && m_omxInit && m_omxDeinit && m_omxComponentNameEnum &&
           m_omxGetHandle && m_omxFreeHandle && m_omxSetupTunnel && m_omxGetComponentsOfRole && m_omxGetRolesOfComponent;
}
