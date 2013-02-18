/* Class TorcOMXBuffer
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

extern "C" {
#include "libavutil/mem.h"
}

TorcOMXBuffer::TorcOMXBuffer(TorcOMXComponent *Parent, OMX_HANDLETYPE Handle, OMX_U32 Port)
  : m_parent(Parent),
    m_handle(Handle),
    m_port(Port),
    m_lock(new QMutex()),
    m_wait(new QWaitCondition()),
    m_createdBuffers(false),
    m_alignment(16)
{
}

TorcOMXBuffer::~TorcOMXBuffer()
{
    Destroy();

    delete m_lock;
    delete m_wait;
}

OMX_ERRORTYPE TorcOMXBuffer::EnablePort(bool Enable)
{
    if (!m_handle || !m_parent)
        return OMX_ErrorUndefined;

    OMX_PARAM_PORTDEFINITIONTYPE portdefinition;
    OMX_INITSTRUCTURE(portdefinition);
    portdefinition.nPortIndex = m_port;

    OMX_ERRORTYPE error = OMX_GetParameter(m_handle, OMX_IndexParamPortDefinition, &portdefinition);
    if (OMX_ErrorNone != error)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("%1: Failed to get port definition").arg(m_parent->GetName()));
        return error;
    }

    if (portdefinition.bEnabled == OMX_FALSE && Enable)
    {
        error = OMX_SendCommand(m_handle, OMX_CommandPortEnable, m_port, NULL);
        if (OMX_ErrorNone != error)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("%1: Failed to send command").arg(m_parent->GetName()));
            return error;
        }

        return m_parent->WaitForResponse(OMX_CommandPortEnable, m_port, 200);
    }
    else if (portdefinition.bEnabled == OMX_TRUE && !Enable)
    {
        error = OMX_SendCommand(m_handle, OMX_CommandPortDisable, m_port, NULL);
        if (OMX_ErrorNone != error)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("%1: Failed to send command").arg(m_parent->GetName()));
            return error;
        }

        return m_parent->WaitForResponse(OMX_CommandPortDisable, m_port, 200);
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE TorcOMXBuffer::Create(bool Allocate)
{
    if (!m_handle)
        return OMX_ErrorUndefined;

    OMX_ERRORTYPE error = EnablePort(true);
    if (OMX_ErrorNone != error)
        return error;

    {
        QMutexLocker locker(m_lock);

        m_createdBuffers = Allocate;
        OMX_PARAM_PORTDEFINITIONTYPE portdefinition;
        OMX_INITSTRUCTURE(portdefinition);
        portdefinition.nPortIndex = m_port;

        error = OMX_GetParameter(m_handle, OMX_IndexParamPortDefinition, &portdefinition);
        if (OMX_ErrorNone != error)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("%1: Failed to get port definition").arg(m_parent->GetName()));
            return error;
        }

        m_alignment = portdefinition.nBufferAlignment;
        if (m_alignment > 32)
        {
            LOG(VB_GENERAL, LOG_WARNING, QString("%1: Port buffer alignment of %1 exceeds libavutil capability")
                .arg(m_parent->GetName()).arg(m_alignment));
        }

        for (OMX_U32 i = 0; i < portdefinition.nBufferCountActual; ++i)
        {
            OMX_BUFFERHEADERTYPE *buffer = NULL;
            OMX_U8               *data   = NULL;

            if (m_createdBuffers)
            {
                data = (OMX_U8*)av_malloc(portdefinition.nBufferSize);
                error = OMX_UseBuffer(m_handle, &buffer, m_port, NULL, portdefinition.nBufferSize, data);
            }
            else
            {
                error = OMX_AllocateBuffer(m_handle, &buffer, m_port, NULL, portdefinition.nBufferSize);
            }

            if (OMX_ErrorNone != error)
            {
                if (m_createdBuffers && data)
                    av_free(data);

                LOG(VB_GENERAL, LOG_ERR, QString("%1: Failed to allocate buffer").arg(m_parent->GetName()));
                return error;
            }

            buffer->pAppPrivate     = (void*)i;
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

OMX_ERRORTYPE TorcOMXBuffer::Destroy(void)
{
    if (!m_handle || !m_parent)
        return OMX_ErrorUndefined;

    (void)EnablePort(false);

    {
        QMutexLocker locker(m_lock);
        for (int i = 0; i < m_buffers.size(); ++i)
        {
            OMX_U8 *buffer = m_buffers.at(i)->pBuffer;

            if (OMX_FreeBuffer(m_handle, m_port, m_buffers.at(i)) != OMX_ErrorNone)
                LOG(VB_GENERAL, LOG_ERR, QString("%1: Error freeing buffer").arg(m_parent->GetName()));

            if (m_createdBuffers)
                av_free(buffer);
        }

        m_buffers.clear();
        m_availableBuffers.clear();
        m_alignment      = 0;
        m_createdBuffers = false;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE TorcOMXBuffer::Flush(void)
{
    if (!m_handle || !m_parent)
        return OMX_ErrorUndefined;

    QMutexLocker locker(m_lock);

    OMX_ERRORTYPE error = OMX_SendCommand(m_handle, OMX_CommandFlush, m_port, NULL);

    if (OMX_ErrorNone != error)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("%1: Failed to send command").arg(m_parent->GetName()));
        return error;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE TorcOMXBuffer::MakeAvailable(OMX_BUFFERHEADERTYPE *Buffer)
{
    m_lock->lock();
    if (!m_buffers.contains(Buffer))
        LOG(VB_GENERAL, LOG_WARNING, QString("%1: Unknown buffer").arg(m_parent->GetName()));
    m_availableBuffers.enqueue(Buffer);
    m_lock->unlock();

    m_wait->wakeAll();
    return OMX_ErrorNone;
}

OMX_BUFFERHEADERTYPE* TorcOMXBuffer::GetBuffer(OMX_U32 Timeout)
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
