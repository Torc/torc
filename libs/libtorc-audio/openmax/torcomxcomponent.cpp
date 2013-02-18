/* Class TorcOMXComponent
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
#include <QElapsedTimer>

// Torc
#include "torclogging.h"
#include "torcomxbuffer.h"
#include "torcomxcomponent.h"

TorcOMXEvent::TorcOMXEvent(OMX_EVENTTYPE Type, OMX_U32 Data1, OMX_U32 Data2)
  : m_type(Type),
    m_data1(Data1),
    m_data2(Data2)
{
}

static OMX_CALLBACKTYPE gCallbacks;

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
    return QString("Unknown");
}

TorcOMXComponent::TorcOMXComponent(TorcOMXCore *Core, OMX_STRING Component, OMX_INDEXTYPE Index)
  : m_valid(false),
    m_core(Core),
    m_handle(NULL),
    m_lock(new QMutex(QMutex::Recursive)),
    m_componentName(Component),
    m_indexType(Index),
    m_inputBuffer(NULL),
    m_outputBuffer(NULL),
    m_eventQueueLock(new QMutex())
{
    gCallbacks.EventHandler    = &EventHandlerCallback;
    gCallbacks.EmptyBufferDone = &EmptyBufferDoneCallback;
    gCallbacks.FillBufferDone  = &FillBufferDoneCallback;

    if (!m_core)
        return;

    OMX_ERRORTYPE status = m_core->m_omxGetHandle(&m_handle, Component, this, &gCallbacks);
    if (status != OMX_ErrorNone || !m_handle)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("%1: Failed to get handle").arg(m_componentName));
        return;
    }

    OMX_PORT_PARAM_TYPE portparameters;
    OMX_INITSTRUCTURE(portparameters);

    if (OMX_GetParameter(m_handle, Index, &portparameters) != OMX_ErrorNone)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("%1: Failed to get port parameters").arg(m_componentName));
        return;
    }

    m_valid = true;
    if (!DisablePorts(OMX_IndexParamVideoInit) || !DisablePorts(OMX_IndexParamAudioInit) ||
        !DisablePorts(OMX_IndexParamImageInit) || !DisablePorts(OMX_IndexParamOtherInit))
    {
        m_valid = false;
        return;
    }

    m_inputBuffer  = new TorcOMXBuffer(this, m_handle, portparameters.nStartPortNumber);
    m_outputBuffer = new TorcOMXBuffer(this, m_handle, std::min(portparameters.nStartPortNumber + 1,
                                       portparameters.nStartPortNumber + portparameters.nPorts - 1));

    LOG(VB_GENERAL, LOG_INFO, QString("%1: Input: %2, Output: %3")
        .arg(m_componentName).arg(m_inputBuffer->m_port).arg(m_outputBuffer->m_port));
}

TorcOMXComponent::~TorcOMXComponent()
{
    {
        QMutexLocker locker(m_lock);

        delete m_inputBuffer;
        delete m_outputBuffer;

        if (m_core && m_handle)
            m_core->m_omxFreeHandle(m_handle);

        m_inputBuffer  = NULL;
        m_outputBuffer = NULL;
        m_handle       = NULL;
    }

    delete m_lock;
    delete m_eventQueueLock;
}

bool TorcOMXComponent::IsValid(void)
{
    return m_valid;
}

OMX_ERRORTYPE TorcOMXComponent::EventHandlerCallback(OMX_HANDLETYPE Component, OMX_PTR OMXComponent, OMX_EVENTTYPE Event, OMX_U32 Data1, OMX_U32 Data2, OMX_PTR EventData)
{
    TorcOMXComponent *component = static_cast<TorcOMXComponent*>(OMXComponent);
    if (component)
        return component->EventHandler(Component, Event, Data1, Data2, EventData);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE TorcOMXComponent::EmptyBufferDoneCallback(OMX_HANDLETYPE Component, OMX_PTR OMXComponent, OMX_BUFFERHEADERTYPE *Buffer)
{
    TorcOMXComponent *component = static_cast<TorcOMXComponent*>(OMXComponent);
    if (component)
        return component->EmptyBufferDone(Component, Buffer);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE TorcOMXComponent::FillBufferDoneCallback(OMX_HANDLETYPE Component, OMX_PTR OMXComponent, OMX_BUFFERHEADERTYPE *Buffer)
{
    TorcOMXComponent *component = static_cast<TorcOMXComponent*>(OMXComponent);
    if (component)
        return component->FillBufferDone(Component, Buffer);

    return OMX_ErrorNone;
}

QString TorcOMXComponent::GetName(void)
{
    return m_componentName;
}

OMX_HANDLETYPE TorcOMXComponent::GetHandle(void)
{
    return m_handle;
}

OMX_ERRORTYPE TorcOMXComponent::SetState(OMX_STATETYPE State)
{
    if (!m_valid)
        return OMX_ErrorUndefined;

    QMutexLocker locker(m_lock);
    OMX_ERRORTYPE error = OMX_SendCommand(m_handle, OMX_CommandStateSet, State, NULL);

    if (OMX_ErrorSameState == error)
    {
        return OMX_ErrorNone;
    }
    else if (OMX_ErrorNone == error)
    {
        error = WaitForResponse(OMX_CommandStateSet, State, 1000);
        if (OMX_ErrorSameState == error)
            error = OMX_ErrorNone;
        return error;
    }

    LOG(VB_GENERAL, LOG_INFO, "3");
    LOG(VB_GENERAL, LOG_ERR, QString("%1: Failed to set state").arg(m_componentName));
    return error;
}

OMX_STATETYPE TorcOMXComponent::GetState(void)
{
    if (!m_valid)
        return OMX_StateInvalid;

    QMutexLocker locker(m_lock);

    OMX_STATETYPE state;
    if (OMX_GetState(m_handle, &state) == OMX_ErrorNone)
        return state;

    LOG(VB_GENERAL, LOG_ERR, QString("%1: Failed to get state").arg(m_componentName));
    return OMX_StateInvalid;
}

OMX_ERRORTYPE TorcOMXComponent::SetParameter(OMX_INDEXTYPE Index, OMX_PTR Structure)
{
    if (!m_valid)
        return OMX_ErrorUndefined;

    QMutexLocker locker(m_lock);

    OMX_ERRORTYPE error = OMX_SetParameter(m_handle, Index, Structure);
    if (OMX_ErrorNone == error)
        return OMX_ErrorNone;

    LOG(VB_GENERAL, LOG_ERR, QString("%1: Failed to set parameter").arg(m_componentName));
    return error;
}

OMX_ERRORTYPE TorcOMXComponent::GetParameter(OMX_INDEXTYPE Index, OMX_PTR Structure)
{
    if (!m_valid)
        return OMX_ErrorUndefined;

    QMutexLocker locker(m_lock);

    OMX_ERRORTYPE error = OMX_GetParameter(m_handle, Index, Structure);
    if (OMX_ErrorNone == error)
        return OMX_ErrorNone;

    LOG(VB_GENERAL, LOG_ERR, QString("%1: Failed to get parameter").arg(m_componentName));
    return error;
}

OMX_ERRORTYPE TorcOMXComponent::SetConfig(OMX_INDEXTYPE Index, OMX_PTR Structure)
{
    if (!m_valid)
        return OMX_ErrorUndefined;

    QMutexLocker locker(m_lock);

    OMX_ERRORTYPE error = OMX_SetConfig(m_handle, Index, Structure);
    if (OMX_ErrorNone == error)
        return OMX_ErrorNone;

    LOG(VB_GENERAL, LOG_ERR, QString("%1: Failed to set config").arg(m_componentName));
    return error;
}

OMX_ERRORTYPE TorcOMXComponent::GetConfig(OMX_INDEXTYPE Index, OMX_PTR Structure)
{
    if (!m_valid)
        return OMX_ErrorUndefined;

    QMutexLocker locker(m_lock);

    OMX_ERRORTYPE error = OMX_GetConfig(m_handle, Index, Structure);
    if (OMX_ErrorNone == error)
        return OMX_ErrorNone;

    LOG(VB_GENERAL, LOG_ERR, QString("%1: Failed to get config").arg(m_componentName));
    return error;
}

OMX_U32 TorcOMXComponent::GetInputPort(void)
{
    if (m_inputBuffer)
        return m_inputBuffer->m_port;
    return 0;
}

OMX_U32 TorcOMXComponent::GetOutputPort(void)
{
    if (m_outputBuffer)
        return m_outputBuffer->m_port;
    return 0;
}

OMX_ERRORTYPE TorcOMXComponent::EnablePort(bool Enable, bool Input)
{
    QMutexLocker locker(m_lock);

    if (Input && m_inputBuffer)
        return m_inputBuffer->EnablePort(Enable);
    else if (!Input && m_outputBuffer)
        return m_outputBuffer->EnablePort(Enable);

    return OMX_ErrorUndefined;
}

OMX_ERRORTYPE TorcOMXComponent::EmptyThisBuffer(OMX_BUFFERHEADERTYPE *Buffer)
{
    if (!m_valid || !Buffer)
        return OMX_ErrorUndefined;

    OMX_ERRORTYPE error = OMX_EmptyThisBuffer(m_handle, Buffer);
    if (OMX_ErrorNone == error)
        return OMX_ErrorNone;

    LOG(VB_GENERAL, LOG_ERR, QString("%1: EmptyThisBuffer failed").arg(m_componentName));
    return error;
}

OMX_ERRORTYPE TorcOMXComponent::FillThisBuffer(OMX_BUFFERHEADERTYPE *Buffer)
{
    if (!m_valid || !Buffer)
        return OMX_ErrorUndefined;

    OMX_ERRORTYPE error = OMX_FillThisBuffer(m_handle, Buffer);
    if (OMX_ErrorNone == error)
        return OMX_ErrorNone;

    LOG(VB_GENERAL, LOG_ERR, QString("%1: FillThisBuffer failed").arg(m_componentName));
    return error;
}

OMX_ERRORTYPE TorcOMXComponent::SetupInputBuffers(bool Create)
{
    OMX_STATETYPE state = GetState();
    if(state != OMX_StateIdle)
    {
        if(state != OMX_StateLoaded)
            SetState(OMX_StateLoaded);
        SetState(OMX_StateIdle);
    }

    if (m_inputBuffer)
        return m_inputBuffer->Create(Create);

    return OMX_ErrorUndefined;
}

OMX_ERRORTYPE TorcOMXComponent::SetupOutputBuffers(bool Create)
{
    OMX_STATETYPE state = GetState();
    if(state != OMX_StateIdle)
    {
        if(state != OMX_StateLoaded)
            SetState(OMX_StateLoaded);
        SetState(OMX_StateIdle);
    }

    if (m_outputBuffer)
        return m_outputBuffer->Create(Create);

    return OMX_ErrorUndefined;
}

OMX_ERRORTYPE TorcOMXComponent::DestroyInputBuffers(void)
{
    if (m_inputBuffer)
        return m_inputBuffer->Destroy();

    return OMX_ErrorUndefined;
}

OMX_ERRORTYPE TorcOMXComponent::DestroyOutputBuffers(void)
{
    if (m_outputBuffer)
        return m_outputBuffer->Destroy();

    return OMX_ErrorUndefined;
}

OMX_BUFFERHEADERTYPE* TorcOMXComponent::GetInputBuffer(OMX_U32 Timeout)
{
    if (m_inputBuffer)
        return m_inputBuffer->GetBuffer(Timeout);
    return NULL;
}

TorcOMXBuffer* TorcOMXComponent::GetInputBuffers(void)
{
    return m_inputBuffer;
}

TorcOMXBuffer* TorcOMXComponent::GetOutputBuffers(void)
{
    return m_outputBuffer;
}

OMX_ERRORTYPE TorcOMXComponent::FlushBuffers(bool Input, bool Output)
{
    OMX_ERRORTYPE error;

    if (Input && m_inputBuffer)
    {
        error = m_inputBuffer->Flush();
        if (OMX_ErrorNone != error)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("%1: Failed to flush input buffers").arg(m_componentName));
            return error;
        }
    }

    if (Output && m_outputBuffer)
    {
        error = m_outputBuffer->Flush();
        if (OMX_ErrorNone != error)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("%1: Failed to flush output buffers").arg(m_componentName));
            return error;
        }
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE TorcOMXComponent::EventHandler(OMX_HANDLETYPE Component, OMX_EVENTTYPE Event, OMX_U32 Data1, OMX_U32 Data2, OMX_PTR EventData)
{
    if (m_handle != Component)
        return OMX_ErrorBadParameter;

    {
        m_eventQueueLock->lock();
        m_eventQueue.append(TorcOMXEvent(Event, Data1, Data2));
        LOG(VB_GENERAL, LOG_DEBUG, QString("Event: %1 %2 %3").arg(EventToString(Event)).arg(Data1).arg(Data2));
        m_eventQueueLock->unlock();
    }

    m_eventQueueWait.wakeAll();

    return OMX_ErrorNone;
}

OMX_ERRORTYPE TorcOMXComponent::EmptyBufferDone(OMX_HANDLETYPE Component, OMX_BUFFERHEADERTYPE *Buffer)
{
    if (m_handle != Component)
        return OMX_ErrorBadParameter;

    if (m_inputBuffer)
        m_inputBuffer->MakeAvailable(Buffer);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE TorcOMXComponent::FillBufferDone(OMX_HANDLETYPE Component, OMX_BUFFERHEADERTYPE *Buffer)
{
    if (m_handle != Component)
        return OMX_ErrorBadParameter;

    if (m_outputBuffer)
        m_outputBuffer->MakeAvailable(Buffer);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE TorcOMXComponent::SendCommand(OMX_COMMANDTYPE Command, OMX_U32 Parameter, OMX_PTR Data)
{
    if (!m_valid)
        return OMX_ErrorUndefined;

    QMutexLocker locker(m_lock);

    OMX_ERRORTYPE error = OMX_SendCommand(m_handle, Command, Parameter, Data);
    if (OMX_ErrorNone == error)
        return OMX_ErrorNone;

    LOG(VB_GENERAL, LOG_ERR, QString("%1: Failed to send command").arg(m_componentName));
    return error;
}

OMX_ERRORTYPE TorcOMXComponent::WaitForResponse(OMX_U32 Command, OMX_U32 Data2, OMX_U32 Timeout)
{
    m_eventQueueLock->lock();

    QElapsedTimer timer;
    timer.start();

    LOG(VB_GENERAL, LOG_DEBUG, QString("%1: Waiting for %2 %3").arg(m_componentName).arg(Command).arg(Data2));

    while (timer.elapsed() < Timeout)
    {
        QList<TorcOMXEvent>::iterator it = m_eventQueue.begin();

        for ( ; it != m_eventQueue.end(); ++it)
        {
            if ((*it).m_type == OMX_EventCmdComplete && (*it).m_data1 == Command && (*it).m_data2 == Data2)
            {
                m_eventQueue.erase(it);
                m_eventQueueLock->unlock();
                return OMX_ErrorNone;
            }
            else if ((*it).m_type == OMX_EventError)
            {
                if ((*it).m_data1 == (OMX_U32)OMX_ErrorSameState && (*it).m_data2 == 1)
                {
                    m_eventQueue.erase(it);
                    m_eventQueueLock->unlock();
                    return OMX_ErrorNone;
                }

                LOG(VB_GENERAL, LOG_ERR, QString("%1: Error event, data1 %2 data2 %3")
                    .arg(m_componentName).arg((*it).m_data1).arg((*it).m_data2));
                OMX_ERRORTYPE error = (OMX_ERRORTYPE)(*it).m_data1;
                m_eventQueue.erase(it);
                m_eventQueueLock->unlock();
                return error;
            }
        }

        m_eventQueueWait.wait(m_eventQueueLock, 50);
    }

    m_eventQueueLock->unlock();
    LOG(VB_GENERAL, LOG_INFO, QString("%1: Response never received").arg(m_componentName));
    return OMX_ErrorMax;
}

bool TorcOMXComponent::DisablePorts(OMX_INDEXTYPE Index)
{
    if (!m_valid)
        return false;

    QMutexLocker locker(m_lock);

    OMX_PORT_PARAM_TYPE portparameters;
    OMX_INITSTRUCTURE(portparameters);

    if (OMX_GetParameter(m_handle, Index, &portparameters) == OMX_ErrorNone)
    {
        for (OMX_U32 i = 0; i < portparameters.nPorts; ++i)
        {
            OMX_PARAM_PORTDEFINITIONTYPE portdefinition;
            OMX_INITSTRUCTURE(portdefinition);
            OMX_U32 portnumber = portparameters.nStartPortNumber + i;
            portdefinition.nPortIndex = portnumber;

            if (OMX_GetParameter(m_handle, OMX_IndexParamPortDefinition, &portdefinition) == OMX_ErrorNone)
            {
                if (OMX_SendCommand(m_handle, OMX_CommandPortDisable, portnumber, NULL) == OMX_ErrorNone)
                    if (WaitForResponse(OMX_CommandPortDisable, portnumber, 100) != OMX_ErrorNone)
                        LOG(VB_GENERAL, LOG_ERR, QString("%1: Failed to disable port").arg(m_componentName));
            }
            else
            {
                portdefinition.bEnabled = OMX_FALSE;
            }
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("%1: Failed to get port parameters").arg(m_componentName));
        return false;
    }

    return true;
}



