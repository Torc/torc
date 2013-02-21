/* Class TorcOMXTunnel
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
#include "torcomxtunnel.h"

TorcOMXTunnel::TorcOMXTunnel(TorcOMXCore *Core, TorcOMXComponent *Source, OMX_U32 SourceIndex, OMX_INDEXTYPE SourceDomain,
                             TorcOMXComponent *Destination, OMX_U32 DestinationIndex, OMX_INDEXTYPE DestinationDomain)
  : m_connected(false),
    m_core(Core),
    m_lock(new QMutex()),
    m_source(Source),
    m_sourceIndex(SourceIndex),
    m_sourceDomain(SourceDomain),
    m_destination(Destination),
    m_destinationIndex(DestinationIndex),
    m_destinationDomain(DestinationDomain)
{
    m_sourcePort      = m_source->GetPort(OMX_DirOutput, m_sourceIndex, m_sourceDomain);
    m_destinationPort = m_destination->GetPort(OMX_DirInput, m_destinationIndex, m_destinationDomain);
}

TorcOMXTunnel::~TorcOMXTunnel()
{
    Destroy();

    delete m_lock;
}

bool TorcOMXTunnel::IsConnected(void)
{
    return m_connected;
}

OMX_ERRORTYPE TorcOMXTunnel::Flush(void)
{
    if (!m_connected)
        return OMX_ErrorUndefined;

    QMutexLocker locker(m_lock);

    OMX_CHECK(m_source->FlushBuffer(OMX_DirOutput, m_sourceIndex, m_sourceDomain), m_source->GetName(), "Tunnel failed to flush source");
    OMX_CHECK(m_destination->FlushBuffer(OMX_DirInput, m_destinationIndex, m_destinationDomain), m_destination->GetName(), "Tunnel failed to flush destination");
    OMX_CHECK(m_source->WaitForResponse(OMX_CommandFlush, m_sourcePort, 200), m_source->GetName(), "Tunnel failed to flush source");
    OMX_CHECK(m_destination->WaitForResponse(OMX_CommandFlush, m_destinationPort, 200), m_destination->GetName(), "Tunnel failed to flush destination");

    return OMX_ErrorNone;
}

OMX_ERRORTYPE TorcOMXTunnel::Create(void)
{
    // TODO add error checking to EnablePort calls

    if (!m_source || !m_destination || !m_core)
        return OMX_ErrorUndefined;

    if (!m_source->GetHandle() || !m_destination->GetHandle() || !m_core->m_omxSetupTunnel)
        return OMX_ErrorUndefined;

    QMutexLocker locker(m_lock);
    m_connected = false;

    if (m_source->GetState() == OMX_StateLoaded)
    {
        OMX_CHECK(m_source->SetState(OMX_StateIdle), m_source->GetName(), "Tunnel failed to set source state");
    }

    m_source->EnablePort(OMX_DirOutput, m_sourceIndex, false, m_sourceDomain);
    m_destination->EnablePort(OMX_DirInput, m_destinationIndex, false, m_destinationDomain);

    OMX_CHECK(m_core->m_omxSetupTunnel(m_source->GetHandle(), m_sourcePort, m_destination->GetHandle(), m_destinationPort), "",
              QString("Failed to create tunnel: %1:%2->%3:%4").arg(m_source->GetName()).arg(m_sourcePort).arg(m_destination->GetName()).arg(m_destinationPort));

    m_source->EnablePort(OMX_DirOutput, m_sourceIndex, true, m_sourceDomain);
    m_destination->EnablePort(OMX_DirInput, m_destinationIndex, true, m_destinationDomain);

    if (m_destination->GetState() == OMX_StateLoaded)
    {
        OMX_CHECK(m_destination->SetState(OMX_StateIdle), m_destination->GetName(), "Tunnel failed to set destination state");
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Created tunnel: %1:%2->%3:%4")
        .arg(m_source->GetName()).arg(m_sourcePort).arg(m_destination->GetName()).arg(m_destinationPort));
    m_connected = true;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE TorcOMXTunnel::Destroy(void)
{
    if (!m_source || !m_destination || !m_core)
        return OMX_ErrorUndefined;

    if (!m_source->GetHandle() || !m_destination->GetHandle() )
    {
        return OMX_ErrorUndefined;
    }

    QMutexLocker locker(m_lock);

    m_source->EnablePort(OMX_DirOutput, m_sourceIndex, false, m_sourceDomain);
    m_destination->EnablePort(OMX_DirInput, m_destinationIndex, false, m_destinationDomain);

    OMX_ERRORTYPE error = m_core->m_omxSetupTunnel(m_source->GetHandle(), m_sourcePort, NULL, 0);
    if (OMX_ErrorNone != error)
        OMX_ERROR(error, m_source->GetName(), "Failed to destroy tunnel input");

    error = m_core->m_omxSetupTunnel(m_destination->GetHandle(), m_destinationPort, NULL, 0);
    if (OMX_ErrorNone != error)
        OMX_ERROR(error, m_destination->GetName(), "Failed to destroy tunnel output");

    m_connected = false;
    return OMX_ErrorNone;
}
