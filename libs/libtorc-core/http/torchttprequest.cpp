/* Class TorcHTTPRequest
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012-13
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
#include <QTextStream>
#include <QStringList>
#include <QDateTime>
#include <QRegExp>
#include <QUrl>

// Torc
#include "version.h"
#include "torclogging.h"
#include "torchttpserver.h"
#include "torcserialiser.h"
#include "torcjsonserialiser.h"
#include "torcxmlserialiser.h"
#include "torcplistserialiser.h"
#include "torcbinaryplistserialiser.h"
#include "torchttprequest.h"

/*! \class TorcHTTPRequest
 *  \brief A class to encapsulte an incoming HTTP request.
 *
 * TorcHTTPRequest validates an incoming HTTP request and prepares the appropriate
 * headers for the response.
 *
 * \sa TorcHTTPServer
 * \sa TorcHTTPHandler
 * \sa TorcHTTPConnection
*/

QRegExp gRegExp = QRegExp("[ \r\n][ \r\n]*");

QString AllowedToString(int Allowed)
{
    QStringList result;

    if (Allowed & HTTPHead)    result << "HEAD";
    if (Allowed & HTTPGet)     result << "GET";
    if (Allowed & HTTPPost)    result << "POST";
    if (Allowed & HTTPPut)     result << "PUT";
    if (Allowed & HTTPDelete)  result << "DELETE";
    if (Allowed & HTTPOptions) result << "OPTONS";

    return result.join(", ");
}

TorcHTTPRequest::TorcHTTPRequest(const QString &Method, QMap<QString,QString> *Headers, QByteArray *Content)
  : m_type(HTTPRequest),
    m_requestType(HTTPUnknownType),
    m_protocol(HTTPUnknownProtocol),
    m_keepAlive(false),
    m_headers(Headers),
    m_content(Content),
    m_allowed(0),
    m_responseType(HTTPResponseUnknown),
    m_responseStatus(HTTP_NotFound),
    m_responseContent(NULL)
{
    QStringList items = Method.split(gRegExp, QString::SkipEmptyParts);
    QString item;

    if (!items.isEmpty())
    {
        item = items.takeFirst();

        if (item == "HTTP/")
        {
            m_type = HTTPResponse;
        }
        else
        {
            m_type = HTTPRequest;
            m_requestType = RequestTypeFromString(item);
        }
    }

    if (!items.isEmpty())
    {
        QUrl url  = QUrl::fromEncoded(items.takeFirst().toUtf8());
        m_path    = url.path();
        m_fullUrl = url.toString();

        int index = m_path.lastIndexOf("/");
        if (index > -1)
        {
            m_method = m_path.mid(index + 1).trimmed();
            m_path   = m_path.left(index + 1).trimmed();
        }

        if (url.hasQuery())
        {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
            QList<QPair<QString, QString> > pairs = url.queryItems();
            for (int i = 0; i < pairs.size(); ++i)
                m_queries.insert(pairs[i].first, pairs[i].second);
#else
            QStringList pairs = url.query().split('&');
            foreach (QString pair, pairs)
            {
                int index = pair.indexOf('=');
                QString key = pair.left(index);
                QString val = pair.mid(index + 1);
                m_queries.insert(key, val);
            }
#endif
        }
    }

    if (!items.isEmpty())
        m_protocol = ProtocolFromString(items.takeFirst());

    m_keepAlive = m_protocol > HTTPOneDotZero;

    QString connection = m_headers->value("connection").toLower();

    if (connection == "keep-alive")
        m_keepAlive = true;
    else if (connection == "close")
        m_keepAlive = false;

    LOG(VB_GENERAL, LOG_DEBUG, QString("HTTP request: path '%1' method '%2'").arg(m_path).arg(m_method));
}

TorcHTTPRequest::~TorcHTTPRequest()
{
    delete m_headers;
    delete m_content;
    delete m_responseContent;
}

bool TorcHTTPRequest::KeepAlive(void)
{
    return m_keepAlive;
}

void TorcHTTPRequest::SetStatus(HTTPStatus Status)
{
    m_responseStatus = Status;
}

void TorcHTTPRequest::SetResponseType(HTTPResponseType Type)
{
    m_responseType = Type;
}

void TorcHTTPRequest::SetResponseContent(QByteArray *Content)
{
    delete m_responseContent;
    m_responseContent = Content;
}

void TorcHTTPRequest::SetAllowed(int Allowed)
{
    m_allowed = Allowed;
}

HTTPType TorcHTTPRequest::GetHTTPType(void)
{
    return m_type;
}

HTTPRequestType TorcHTTPRequest::GetHTTPRequestType(void)
{
    return m_requestType;
}

QString TorcHTTPRequest::GetUrl(void)
{
    return m_fullUrl;
}

QString TorcHTTPRequest::GetPath(void)
{
    return m_path;
}

QString TorcHTTPRequest::GetMethod(void)
{
    return m_method;
}

QMap<QString,QString> TorcHTTPRequest::Queries(void)
{
    return m_queries;
}

QPair<QByteArray*,QByteArray*> TorcHTTPRequest::Respond(void)
{
    if (m_responseType == HTTPResponseUnknown)
    {
        LOG(VB_GENERAL, LOG_ERR, "Unknown HTTP response");
        m_responseStatus = HTTP_InternalServerError;
        m_responseType   = HTTPResponseDefault;
        m_keepAlive      = false;
    }

    QByteArray *buffer = new QByteArray();
    QTextStream response(buffer);

    QString contenttype = ResponseTypeToString(m_responseType);

    response << TorcHTTPRequest::ProtocolToString(m_protocol) << " " << TorcHTTPRequest::StatusToString(m_responseStatus) << "\r\n";
    response << "Date: " << QDateTime::currentDateTimeUtc().toString("d MMM yyyy hh:mm:ss 'GMT'") << "\r\n";
    response << "Server: " << TorcHTTPServer::PlatformName() << ", Torc " << TORC_SOURCE_VERSION << "\r\n";
    response << "Connection: " << (m_keepAlive ? QString("keep-alive") : QString("close")) << "\r\n";
    response << "Accept-Ranges: bytes\r\n";
    response << "Content-Length: " << (m_responseContent ? QString::number(m_responseContent->size()) : "0") << "\r\n";

    if (!contenttype.isEmpty())
        response << "Content-Type: " << contenttype << "\r\n";

    if (m_allowed)
        response << "Allow: " << AllowedToString(m_allowed) << "\r\n";

    response << "\r\n";

    return QPair<QByteArray*,QByteArray*>(buffer, m_responseContent);
}

HTTPRequestType TorcHTTPRequest::RequestTypeFromString(const QString &Type)
{
    if (Type == "GET")     return HTTPGet;
    if (Type == "HEAD")    return HTTPHead;
    if (Type == "POST")    return HTTPPost;
    if (Type == "PUT")     return HTTPPut;
    if (Type == "OPTIONS") return HTTPOptions;
    if (Type == "DELETE")  return HTTPDelete;

    return HTTPUnknownType;
}

HTTPProtocol TorcHTTPRequest::ProtocolFromString(const QString &Protocol)
{
    if (Protocol.startsWith("HTTP"))
    {
        if (Protocol.endsWith("1.1")) return HTTPOneDotOne;
        if (Protocol.endsWith("1.0")) return HTTPOneDotZero;
        if (Protocol.endsWith("0.9")) return HTTPZeroDotNine;
    }

    return HTTPUnknownProtocol;
}

QString TorcHTTPRequest::ProtocolToString(HTTPProtocol Protocol)
{
    switch (Protocol)
    {
        case HTTPOneDotOne:       return QString("HTTP/1.1");
        case HTTPOneDotZero:      return QString("HTTP/1.0");
        case HTTPZeroDotNine:     return QString("HTTP/0.9");
        case HTTPUnknownProtocol: return QString("Error");
    }

    return QString("Error");
}

QString TorcHTTPRequest::StatusToString(HTTPStatus Status)
{
    switch (Status)
    {
        case HTTP_OK:                  return QString("200 OK");
        case HTTP_BadRequest:          return QString("400 Bad Request");
        case HTTP_Unauthorized:        return QString("401 Unauthorized");
        case HTTP_Forbidden:           return QString("403 Forbidden");
        case HTTP_NotFound:            return QString("404 Not Found");
        case HTTP_MethodNotAllowed:    return QString("405 Method Not Allowed");
        case HTTP_InternalServerError: return QString("500 Internal Server Error");
    }

    return QString("Error");
}

QString TorcHTTPRequest::ResponseTypeToString(HTTPResponseType Response)
{
    switch (Response)
    {
        case HTTPResponseXML:              return QString("text/xml; charset=\"UTF-8\"");
        case HTTPResponseHTML:             return QString("text/html; charset=\"UTF-8\"");
        case HTTPResponseJSON:             return QString("application/json");
        case HTTPResponseJSONJavascript:   return QString("text/javascript");
        case HTTPResponsePList:            return QString("application/plist");
        case HTTPResponseBinaryPList:      return QString("application/x-plist");
        case HTTPResponsePListApple:       return QString("text/x-apple-plist+xml");
        case HTTPResponseBinaryPListApple: return QString("application/x-apple-binary-plist");
        default: break;
    }

    return QString("text/plain");
}

TorcSerialiser* TorcHTTPRequest::GetSerialiser(void)
{
    QString accept = m_headers->value("Accept");

    if (accept.contains("application/json", Qt::CaseInsensitive))
        return new TorcJSONSerialiser();

    if (accept.contains("text/javascript", Qt::CaseInsensitive))
        return new TorcJSONSerialiser(true /*javascript*/);

    if (accept.contains("text/x-apple-plist+xml", Qt::CaseInsensitive))
        return new TorcPListSerialiser();

    if (accept.contains("application/plist", Qt::CaseInsensitive))
        return new TorcPListSerialiser();

    if (accept.contains("application/x-plist", Qt::CaseInsensitive))
        return new TorcBinaryPListSerialiser();

    if (accept.contains("application/x-apple-binary-plist", Qt::CaseInsensitive))
        return new TorcBinaryPListSerialiser();

    return new TorcXMLSerialiser();
}
