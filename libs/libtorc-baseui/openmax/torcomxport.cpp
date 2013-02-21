/* Class TorcOMXPort
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
#include "torcomxport.h"

TorcOMXPort::TorcOMXPort(TorcOMXComponent *Parent, OMX_HANDLETYPE Handle, OMX_U32 Port, OMX_INDEXTYPE Domain)
  : m_parent(Parent),
    m_handle(Handle),
    m_port(Port),
    m_domain(Domain),
    m_lock(new QMutex()),
    m_wait(new QWaitCondition()),
    m_alignment(16)
{
}

TorcOMXPort::~TorcOMXPort()
{
    DestroyBuffers();

    delete m_lock;
    delete m_wait;
}

OMX_U32 TorcOMXPort::GetPort(void)
{
    return m_port;
}

OMX_INDEXTYPE TorcOMXPort::GetDomain(void)
{
    return m_domain;
}

OMX_ERRORTYPE TorcOMXPort::EnablePort(bool Enable)
{
    if (!m_handle || !m_parent)
        return OMX_ErrorUndefined;

    OMX_PARAM_PORTDEFINITIONTYPE portdefinition;
    OMX_INITSTRUCTURE(portdefinition);
    portdefinition.nPortIndex = m_port;

    OMX_ERRORTYPE error = OMX_GetParameter(m_handle, OMX_IndexParamPortDefinition, &portdefinition);
    OMX_CHECK(error, m_parent->GetName(), "Failed to get port definition");

    if (portdefinition.bEnabled == OMX_FALSE && Enable)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("%1: Enabling port %2").arg(m_parent->GetName()).arg(m_port));
        error = OMX_SendCommand(m_handle, OMX_CommandPortEnable, m_port, NULL);
        OMX_CHECK(error, m_parent->GetName(), "Failed to send command");
        return m_parent->WaitForResponse(OMX_CommandPortEnable, m_port, 1000);
    }
    else if (portdefinition.bEnabled == OMX_TRUE && !Enable)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("%1: Disabling port %2").arg(m_parent->GetName()).arg(m_port));
        error = OMX_SendCommand(m_handle, OMX_CommandPortDisable, m_port, NULL);
        OMX_CHECK(error, m_parent->GetName(), "Failed to send command");
        return m_parent->WaitForResponse(OMX_CommandPortDisable, m_port, 1000);
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE TorcOMXPort::CreateBuffers(void)
{
    if (!m_handle)
        return OMX_ErrorUndefined;

    OMX_ERRORTYPE error = EnablePort(true);
    if (OMX_ErrorNone != error)
        return error;

    {
        QMutexLocker locker(m_lock);

        OMX_PARAM_PORTDEFINITIONTYPE portdefinition;
        OMX_INITSTRUCTURE(portdefinition);
        portdefinition.nPortIndex = m_port;

        error = OMX_GetParameter(m_handle, OMX_IndexParamPortDefinition, &portdefinition);
        OMX_CHECK(error, m_parent->GetName(), "Failed to get port definition");

        m_alignment = portdefinition.nBufferAlignment;

        for (OMX_U32 i = 0; i < portdefinition.nBufferCountActual; ++i)
        {
            OMX_BUFFERHEADERTYPE *buffer = NULL;
            error = OMX_AllocateBuffer(m_handle, &buffer, m_port, NULL, portdefinition.nBufferSize);
            if (OMX_ErrorNone != error)
            {
                OMX_ERROR(error, m_parent->GetName(), "Failed to allocate buffer");
                return error;
            }

            buffer->pAppPrivate     = (void*)this;
            buffer->nFilledLen      = 0;
            buffer->nOffset         = 0;
            buffer->nInputPortIndex = m_port;

            m_buffers.append(buffer);
            m_availableBuffers.enqueue(buffer);
        }

        LOG(VB_GENERAL, LOG_INFO, QString("%1: Created %2 %3byte buffers")
            .arg(m_parent->GetName()).arg(portdefinition.nBufferCountActual).arg(portdefinition.nBufferSize));
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE TorcOMXPort::DestroyBuffers(void)
{
    if (!m_handle || !m_parent)
        return OMX_ErrorUndefined;

    (void)EnablePort(false);

    {
        QMutexLocker locker(m_lock);
        for (int i = 0; i < m_buffers.size(); ++i)
        {
            OMX_ERRORTYPE error = OMX_FreeBuffer(m_handle, m_port, m_buffers.at(i));
            OMX_CHECKX(error, m_parent->GetName(), "Failed to free buffer");
        }

        m_buffers.clear();
        m_availableBuffers.clear();
        m_alignment      = 0;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE TorcOMXPort::Flush(void)
{
    if (!m_handle || !m_parent)
        return OMX_ErrorUndefined;

    QMutexLocker locker(m_lock);

    OMX_ERRORTYPE error = OMX_SendCommand(m_handle, OMX_CommandFlush, m_port, NULL);
    OMX_CHECK(error, m_parent->GetName(), "Failed to send command");

    return OMX_ErrorNone;
}

OMX_ERRORTYPE TorcOMXPort::MakeAvailable(OMX_BUFFERHEADERTYPE *Buffer)
{
    m_lock->lock();
    if (!m_buffers.contains(Buffer))
        LOG(VB_GENERAL, LOG_WARNING, QString("%1: Unknown buffer").arg(m_parent->GetName()));
    m_availableBuffers.enqueue(Buffer);
    m_lock->unlock();

    m_wait->wakeAll();
    return OMX_ErrorNone;
}

OMX_BUFFERHEADERTYPE* TorcOMXPort::GetBuffer(OMX_S32 Timeout)
{
    OMX_BUFFERHEADERTYPE* result = NULL;

    m_lock->lock();

    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < Timeout)
    {
        if (!m_availableBuffers.isEmpty())
        {
            result = m_availableBuffers.dequeue();
            m_lock->unlock();
            return result;
        }

        m_wait->wait(m_lock, 10);
    }

    m_lock->unlock();
    LOG(VB_GENERAL, LOG_ERR, QString("%1: Timed out waiting for available buffer").arg(m_parent->GetName()));
    return result;
}
