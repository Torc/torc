/* Class TorcRPCRequest
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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

// Qt
#include <QJsonObject>
#include <QJsonDocument>

// Torc
#include "torclogging.h"
#include "torcrpcrequest.h"

/*! \class TorcRPCRequest
 *  \brief A class encapsulating a Remote Procedure Call.
 *
 * Remote Procedure Calls are currently only handled through TorcWebSocket.
 *
 * \sa TorcWebSocket
 *
 * \todo Add serialisation of protocols other than JSON-RPC.
 * \todo Add parsing of protocols other than JSON-RPC.
 * \todo Review memory consumption/performance.
*/

/*! \brief Creates an RPC request owned by the given Parent.
 *
 * Parent will be notified when the request has been completed (or on error) and is
 * responsible for cancelling the request if it is no longer required. Upon completion,
 * error or cancellation, the parent must then DownRef the request to ensure it is
 * deleted.
*/
TorcRPCRequest::TorcRPCRequest(const QString &Method, QObject *Parent)
  : m_state(None),
    m_id(-1),
    m_method(Method),
    m_parent(Parent),
    m_validParent(true)
{
    if (m_parent->metaObject()->indexOfMethod(QMetaObject::normalizedSignature("RequestReady(TorcRPCRequest*)")) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, "Request's parent does not have RequestReady method - request WILL fail");
        m_validParent = false;
    }
}

/*! \brief Creates a notification request for which no response is expected.
 *
 * Notification requests are deleted after they have been sent.
*/
TorcRPCRequest::TorcRPCRequest(const QString &Method)
  : m_state(None),
    m_id(-1),
    m_method(Method),
    m_parent(NULL)
{
}

TorcRPCRequest::~TorcRPCRequest()
{
}

///\brief Parse the contents of Payload, assuming data consistent with Protocol.
QVariant TorcRPCRequest::ParsePayload(TorcWebSocket::WSSubProtocol Protocol, const QByteArray &Payload)
{
    if (Protocol == TorcWebSocket::SubProtocolJSONRPC)
    {
        QJsonDocument doc = QJsonDocument::fromJson(Payload);
        if (!doc.isNull())
        {
            LOG(VB_GENERAL, LOG_INFO, Payload);
            return doc.toVariant();
        }

        LOG(VB_GENERAL, LOG_ERR, "Error parsing response to JSON-RPC");
    }

    return QVariant();
}

///\brief Signal to the parent that the request is ready (but may be errored).
void TorcRPCRequest::NotifyParent(void)
{
    if (!m_parent || !m_validParent || m_state & Cancelled)
        return;

    QMetaObject::invokeMethod(m_parent, "RequestReady", Q_ARG(TorcRPCRequest*, this));
}

///\brief Serialise the request for the given protocol.
QByteArray& TorcRPCRequest::SerialiseRequest(TorcWebSocket::WSSubProtocol Protocol)
{
    if (Protocol == TorcWebSocket::SubProtocolJSONRPC)
    {
        QJsonObject object;
        object.insert("jsonrpc", QString("2.0"));
        object.insert("method",  m_method);

        if (!m_parameters.isEmpty())
        {
            QJsonObject params;
            for (int i = 0; i < m_parameters.size(); ++i)
                params.insert(m_parameters[i].first, m_parameters[i].second.toString());
            object.insert("params", params);
        }

        if (m_parent && m_id > -1)
            object.insert("id", m_id);

        QJsonDocument doc(object);
        m_serialisedData = doc.toJson();
    }

    LOG(VB_NETWORK, LOG_DEBUG, m_serialisedData);

    return m_serialisedData;
}

///\brief Serialise the response for the given protocol
QByteArray TorcRPCRequest::SerialiseResponse(TorcWebSocket::WSSubProtocol Protocol, QVariantMap &Response)
{
    if (Protocol == TorcWebSocket::SubProtocolJSONRPC)
    {
        Response.insert("jsonrpc", QString("2.0"));
        QJsonDocument doc = QJsonDocument::fromVariant(Response);
        return doc.toJson();
    }

    return QByteArray();
}

/*! \brief Progress the state for this request.
 *
 * \note Request state is incremental.
*/
void TorcRPCRequest::AddState(int State)
{
    m_state |= State;
}

///\brief Set the unique ID for this request.
void TorcRPCRequest::SetID(int ID)
{
    m_id = ID;
}

/*! \brief Add Parameter and value to list of call parameters.
 *
 * \todo Check for duplicate parameters.
*/
void TorcRPCRequest::AddParameter(const QString &Name, const QVariant &Value)
{
    m_parameters.append(QPair<QString,QVariant>(Name, Value));
}

void TorcRPCRequest::SetReply(const QVariant &Reply)
{
    m_reply = Reply;
}

int TorcRPCRequest::GetState(void)
{
    return m_state;
}

int TorcRPCRequest::GetID(void)
{
    return m_id;
}

QString TorcRPCRequest::GetMethod(void)
{
    return m_method;
}

QObject* TorcRPCRequest::GetParent(void)
{
    return m_parent;
}

const QVariant& TorcRPCRequest::GetReply(void)
{
    return m_reply;
}

const QList<QPair<QString,QVariant> >& TorcRPCRequest::GetParameters(void)
{
    return m_parameters;
}
