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
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

// Torc
#include "torclogging.h"
#include "http/torchttpserver.h"
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
    m_parent(NULL),
    m_validParent(false)
{
}

/*! \brief Creates a request or response from the given raw data using the given protocol.
 *
*/
TorcRPCRequest::TorcRPCRequest(TorcWebSocket::WSSubProtocol Protocol, const QByteArray &Data)
  : m_state(None),
    m_id(-1),
    m_method(),
    m_parent(NULL),
    m_validParent(false)
{
    if (Protocol == TorcWebSocket::SubProtocolJSONRPC)
    {
        // parse the JSON
        QJsonDocument doc = QJsonDocument::fromJson(Data);

        if (doc.isNull())
        {
            // NB we are acting as both client and server, hence if we receive invalid JSON (that Qt cannot parse)
            // we can only make a best efforts guess as to whether this was a request. Hence under
            // certain circumstances, we may not send the appropriate error message or respond at all.
            if (Data.contains("method"))
            {
                QJsonObject object;
                QJsonObject error;
                error.insert("code", -32700);
                error.insert("message", QString("Parse error"));

                object.insert("error",   error);
                object.insert("jsonrpc", QString("2.0"));
                object.insert("id",      QJsonValue());

                m_serialisedData = QJsonDocument(object).toJson();

                LOG(VB_GENERAL, LOG_INFO, m_serialisedData);
            }

            LOG(VB_GENERAL, LOG_ERR, "Error parsing JSON-RPC data");
            AddState(Errored);
            return;
        }

        LOG(VB_GENERAL, LOG_INFO, Data);

        // single request, one JSON object
        if (doc.isObject())
        {
            QJsonObject object = doc.object();

            // determine whether this is a request of response
            int  id        = (object.contains("id") && !object["id"].isNull()) ? (int)object["id"].toDouble() : -1;
            bool isrequest = object.contains("method");
            bool isresult  = object.contains("result");
            bool iserror   = object.contains("error");

            if ((int)isrequest + (int)isresult + (int)iserror != 1)
            {
                LOG(VB_GENERAL, LOG_ERR, "Ambiguous RPC request/response");
                AddState(Errored);
                return;
            }

            if (isrequest)
            {
                QVariantMap result = TorcHTTPServer::HandleRequest(object["method"].toString(), object.value("params").toVariant());

                // not a notification, response expected
                if (id > -1)
                {
                    // result should contain either 'result' or 'error', we need to insert id and protocol identifier
                    result.insert("jsonrpc", QString("2.0"));
                    result.insert("id", id);
                    m_serialisedData = QJsonDocument::fromVariant(result).toJson();
                }
                else if (object.contains("id"))
                {
                    LOG(VB_GENERAL, LOG_ERR, "Request contains invalid id");
                }
            }
            else if (isresult)
            {
                m_reply = object.value("result").toVariant();
                AddState(Result);
                m_id = id;

                if (m_id < 0)
                {
                    LOG(VB_GENERAL, LOG_ERR, "Received result with no id");
                    AddState(Errored);
                }
            }
            else if (iserror)
            {
                AddState(Errored);
                AddState(Result);
                m_id = id;

                if (m_id < 0)
                    LOG(VB_GENERAL, LOG_ERR, "Received error with no id");
            }
        }
        else if (doc.isArray())
        {
            QJsonObject error;
            QJsonObject object;
            object.insert("code",    -32600);
            object.insert("message", QString("Invalid request"));
            error.insert("error",    object);
            error.insert("jsonrpc",  QString("2.0"));
            error.insert("id",       QJsonValue());

            // this must be a batched request
            QJsonArray array = doc.array();

            // an empty array is an error
            if (array.isEmpty())
            {
                m_serialisedData = QJsonDocument(error).toJson();
                LOG(VB_GENERAL, LOG_ERR, "Invalid request - empty array");
                return;
            }

            // iterate over each member
            QByteArray result;
            result.append("[\r\n");
            bool first = true;

            QJsonArray::iterator it = array.begin();
            for ( ; it != array.end(); ++it)
            {
                // must be an object
                if (!(*it).isObject())
                {
                    LOG(VB_GENERAL, LOG_ERR, "Invalid request - not an object");
                    result.append(QJsonDocument(error).toJson());
                    continue;
                }

                // process this object
                // NB Converting from Json back to raw data and back to Json is hopelessly inefficient
                QByteArray data = QJsonDocument((*it).toObject()).toJson();
                TorcRPCRequest *request = new TorcRPCRequest(Protocol, data);

                if (!request->GetData().isEmpty())
                {
                    if (first)
                        first = false;
                    else
                        result.append(",\r\n");
                    result.append(request->GetData());
                }

                request->DownRef();
            }

            result.append("\r\n]");

            // don't return an empty array
            if (!first)
                m_serialisedData = result;
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "Unknown JsonDocument type");
        }
    }
}

TorcRPCRequest::~TorcRPCRequest()
{
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

QByteArray& TorcRPCRequest::GetData(void)
{
    return m_serialisedData;
}
