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

QString EventToString(OMX_EVENTTYPE Event)
{
    switch (Event)
    {
        case OMX_EventCmdComplete:               return QString("CmdComplete");
        case OMX_EventError:                     return QString("Error");
        case OMX_EventMark:                      return QString("Mark");
        case OMX_EventPortSettingsChanged:       return QString("PortSettingsChanged");
        case OMX_EventBufferFlag:                return QString("BufferFlag");
        case OMX_EventResourcesAcquired:         return QString("ResourcesAcquired");
        case OMX_EventComponentResumed:          return QString("ComponentResumed");
        case OMX_EventDynamicResourcesAvailable: return QString("DynamicResourcesAvailable");
        case OMX_EventPortFormatDetected:        return QString("PortFormatDetected");
        default: break;
    }

    return QString("Unknown %1").arg(Event);
}

QString StateToString(OMX_STATETYPE State)
{
    switch (State)
    {
        case OMX_StateInvalid:          return QString("Invalid");
        case OMX_StateLoaded:           return QString("Loaded");
        case OMX_StateIdle:             return QString("Idle");
        case OMX_StateExecuting:        return QString("Executing");
        case OMX_StatePause:            return QString("Pause");
        case OMX_StateWaitForResources: return QString("WaitForResources");
        default: break;
    }
    return QString("Unknown %1").arg(State);
}

QString ErrorToString(OMX_ERRORTYPE Error)
{
    switch (Error)
    {
        case OMX_ErrorNone:                               return QString("None");
        case OMX_ErrorInsufficientResources:              return QString("InsufficientResources");
        case OMX_ErrorUndefined:                          return QString("Undefined");
        case OMX_ErrorInvalidComponentName:               return QString("InvalidComponentName");
        case OMX_ErrorComponentNotFound:                  return QString("ComponentNotFound");
        case OMX_ErrorInvalidComponent:                   return QString("InvalidComponent");
        case OMX_ErrorBadParameter:                       return QString("BadParameter");
        case OMX_ErrorNotImplemented:                     return QString("NotImplemented");
        case OMX_ErrorUnderflow:                          return QString("Underflow");
        case OMX_ErrorOverflow:                           return QString("Overflow");
        case OMX_ErrorHardware:                           return QString("Hardware");
        case OMX_ErrorInvalidState:                       return QString("InvalidState");
        case OMX_ErrorStreamCorrupt:                      return QString("StreamCorrupt");
        case OMX_ErrorPortsNotCompatible:                 return QString("PortsNotCompatible");
        case OMX_ErrorResourcesLost:                      return QString("ResourcesLost");
        case OMX_ErrorNoMore:                             return QString("NoMore");
        case OMX_ErrorVersionMismatch:                    return QString("VersionMismatch");
        case OMX_ErrorNotReady:                           return QString("NotReady");
        case OMX_ErrorTimeout:                            return QString("Timeout");
        case OMX_ErrorSameState:                          return QString("SameState");
        case OMX_ErrorResourcesPreempted:                 return QString("ResourcesPreempted");
        case OMX_ErrorPortUnresponsiveDuringAllocation:   return QString("PortUnresponsiveDuringAllocation");
        case OMX_ErrorPortUnresponsiveDuringDeallocation: return QString("PortUnresponsiveDuringDeallocation");
        case OMX_ErrorPortUnresponsiveDuringStop:         return QString("PortUnresponsiveDuringStop");
        case OMX_ErrorIncorrectStateTransition:           return QString("IncorrectStateTransition");
        case OMX_ErrorIncorrectStateOperation:            return QString("IncorrectStateOperation");
        case OMX_ErrorUnsupportedSetting:                 return QString("UnsupportedSetting");
        case OMX_ErrorUnsupportedIndex:                   return QString("UnsupportedIndex");
        case OMX_ErrorBadPortIndex:                       return QString("BadPortIndex");
        case OMX_ErrorPortUnpopulated:                    return QString("PortUnpopulated");
        case OMX_ErrorComponentSuspended:                 return QString("ComponentSuspended");
        case OMX_ErrorDynamicResourcesUnavailable:        return QString("DynamicResourcesUnavailable");
        case OMX_ErrorMbErrorsInFrame:                    return QString("MbErrorsInFrame");
        case OMX_ErrorFormatNotDetected:                  return QString("FormatNotDetected");
        case OMX_ErrorContentPipeOpenFailed:              return QString("ContentPipeOpenFailed");
        case OMX_ErrorContentPipeCreationFailed:          return QString("ContentPipeCreationFailed");
        case OMX_ErrorSeperateTablesUsed:                 return QString("SeperateTablesUsed");
        case OMX_ErrorTunnelingUnsupported:               return QString("TunnelingUnsupported");
        case OMX_ErrorMax:                                return QString("Max");
        default: break;
    }

    return QString("Unknown error 0x%1").arg(Error, 0, 16);
}

QString CommandToString(OMX_COMMANDTYPE Command)
{
    switch (Command)
    {
        case OMX_CommandStateSet:    return QString("StateSet");
        case OMX_CommandFlush:       return QString("Flush");
        case OMX_CommandPortDisable: return QString("PortDisable");
        case OMX_CommandPortEnable:  return QString("PortEnable");
        case OMX_CommandMarkBuffer:  return QString("MarkBuffer");
        case OMX_CommandMax:         return QString("Max");
        default: break;
    }

    return QString("Unknown %1").arg(Command);
}

QString DomainToString(OMX_INDEXTYPE Domain)
{
    switch (Domain)
    {
        case OMX_IndexParamAudioInit: return QString("Audio");
        case OMX_IndexParamImageInit: return QString("Image");
        case OMX_IndexParamVideoInit: return QString("Video");
        case OMX_IndexParamOtherInit: return QString("Other");
        default: break;
    }

    return QString("Unknown");
}

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
