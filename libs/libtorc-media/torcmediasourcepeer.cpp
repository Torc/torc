/* Class TorcMediaSourcePeer
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
#include "torclocalcontext.h"
#include "torcrpcrequest.h"
#include "torcnetworkedcontext.h"
#include "torcmediasource.h"
#include "torcmediasourcepeer.h"

/*! \class TorcMediaPeer
 *  \brief A simple wrapper around a Torc peer connection.
*/
class TorcMediaPeer
{
  public:
    TorcMediaPeer(TorcMediaSourcePeer *Parent, const QString &UUID, const QString &Name)
      : m_parent(Parent),
        m_name(Name),
        m_uuid(UUID),
        m_currentRequest(NULL)
    {
    }

    ~TorcMediaPeer()
    {
        CancelRequest();
    }

    /*! \brief Begin querying the peer for exported media files.
    */
    void Start(void)
    {
        if (!m_parent)
            return;

        m_currentRequest = new TorcRPCRequest("/services/files/GetMediaVersion", m_parent);
        TorcNetworkedContext::RemoteRequest(m_uuid, m_currentRequest);
    }

    void ProcessRequest(TorcRPCRequest *Request)
    {
        if (m_currentRequest && Request == m_currentRequest)
        {
            m_currentRequest->DownRef();
            m_currentRequest = NULL;
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "Cannot process unknown request");
        }
    }

    /*! \brief Cancel the current remote request.
     *
     * \note Cancelling a request will involve 3 threads (MediaLoop, MainLoop and WebSocket) and
     * 2 asynchronous calls (TorcNetworkedContext::CancelRequest and TorcWebSocket::CancelRequest),
     * both of which will block a little to wait for the cancellation to be confirmed. This avoids
     * leaks.
     *
     * \sa TorcNetworkedContext::CancelRequest
     * \sa TorcWebSocket::CancelRequest
    */
    void CancelRequest(void)
    {
        if (m_currentRequest)
        {
            m_currentRequest->SetParent(NULL);
            TorcNetworkedContext::CancelRequest(m_uuid, m_currentRequest);
            m_currentRequest->DownRef();
            m_currentRequest = NULL;
        }
    }

    TorcMediaSourcePeer *m_parent;
    QString              m_name;
    QString              m_uuid;
    TorcRPCRequest      *m_currentRequest;
};

/*! \class TorcMediaSourcePeer
 *  \brief A singleton class to interrogate known Torc peers for media files.
 *
 * New peer connections are signalled by TorcNetworkedContext, at which point a subscription
 * request is sent to the remote 'files' service.
 *
 * \sa TorcMediaSource
 * \sa TorcMediaSourceDirectory
 * \sa TorcNetworkedContext
 * \sa TorcNetworkService
*/
TorcMediaSourcePeer::TorcMediaSourcePeer()
  : QObject()
{
    // listen for peer (dis)connection signals
    if (gNetworkedContext)
    {
        connect(gNetworkedContext, SIGNAL(PeerConnected(QString,QString)),    this, SLOT(PeerConnected(QString,QString)));
        connect(gNetworkedContext, SIGNAL(PeerDisconnected(QString,QString)), this, SLOT(PeerDisconnected(QString,QString)));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "No TorcNetworkedContext singleton to connect to");
    }
}

TorcMediaSourcePeer::~TorcMediaSourcePeer()
{
    // stop listening for peer updates
    disconnect();

    // delete peers
    qDeleteAll(m_peers);
}

///\brief A WebSocket has been established with the peer identified by UUID
void TorcMediaSourcePeer::PeerConnected(QString Name, QString UUID)
{
    if (m_peers.contains(UUID))
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("Received connection signal for known peer - ignoring (%1, %2)").arg(Name).arg(UUID));
        return;
    }

    TorcMediaPeer *handler = new TorcMediaPeer(this, UUID, Name);
    m_peers.insert(UUID, handler);
    handler->Start();
}

///\brief The WebSocket connection identified by UUID has been closed/disconnected.
void TorcMediaSourcePeer::PeerDisconnected(QString Name, QString UUID)
{
    if (!m_peers.contains(UUID))
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("Received disconnection signal for unknown peer - ignoring (%1, %2)").arg(Name).arg(UUID));
        return;
    }

    delete m_peers.take(UUID);
}

void TorcMediaSourcePeer::RequestReady(TorcRPCRequest *Request)
{
    if (!Request)
        return;

    QMap<QString,TorcMediaPeer*>::const_iterator it = m_peers.constBegin();
    for ( ; it != m_peers.constEnd(); ++it)
    {
        if (it.value()->m_currentRequest == Request)
        {
            it.value()->ProcessRequest(Request);
            return;
        }
    }
}

/*! \class TorcMediaSourcePeerObject
 *  \brief Creates the singleton TorcMediaSourcePeer object in the MediaLoop thread.
 *
 * \sa TorcMediaSource
*/
class TorcMediaSourcePeerObject : public TorcMediaSource
{
  public:
    TorcMediaSourcePeerObject()
     :  TorcMediaSource(),
        m_peerMonitor(NULL)
    {
    }

    void Create(void)
    {
        m_peerMonitor = new TorcMediaSourcePeer();
    }

    void Destroy(void)
    {
        delete m_peerMonitor;
    }

  private:
    TorcMediaSourcePeer *m_peerMonitor;

} TorcMediaSourcePeerObject;

