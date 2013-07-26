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
#include <QScopedPointer>
#include <QTcpSocket>
#include <QTextStream>
#include <QStringList>
#include <QDateTime>
#include <QRegExp>
#include <QFile>
#include <QUrl>

// Torc
#include "version.h"
#include "torclogging.h"
#include "torcmime.h"
#include "torchttpserver.h"
#include "torcserialiser.h"
#include "torcjsonserialiser.h"
#include "torcxmlserialiser.h"
#include "torcplistserialiser.h"
#include "torcbinaryplistserialiser.h"
#include "torchttprequest.h"

#ifdef __linux__
#include <sys/sendfile.h>
#endif

/*! \class TorcHTTPRequest
 *  \brief A class to encapsulte an incoming HTTP request.
 *
 * TorcHTTPRequest validates an incoming HTTP request and prepares the appropriate
 * headers for the response.
 *
 * \sa TorcHTTPServer
 * \sa TorcHTTPHandler
 * \sa TorcHTTPConnection
 *
 * \todo Break out writes into 'small' chunks to facilitate abort handling.
 * \todo Add linux sendfile support with fallback to regular writes.
*/

QRegExp gRegExp = QRegExp("[ \r\n][ \r\n]*");

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
    m_responseContent(NULL),
    m_responseFile(NULL)
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
            QStringList pairs = url.query().split('&');
            foreach (QString pair, pairs)
            {
                int index = pair.indexOf('=');
                QString key = pair.left(index);
                QString val = pair.mid(index + 1);
                m_queries.insert(key, val);
            }
        }
    }

    if (!items.isEmpty())
        m_protocol = ProtocolFromString(items.takeFirst());

    m_keepAlive = m_protocol > HTTPOneDotZero;

    QString connection = m_headers->value("Connection").toLower();

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

    if (m_responseFile)
        m_responseFile->close();
    delete m_responseFile;
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
    if (m_responseFile)
        m_responseFile->close();

    delete m_responseFile;
    delete m_responseContent;
    m_responseContent = Content;
    m_responseFile    = NULL;
}

void TorcHTTPRequest::SetResponseFile(QFile *File)
{
    if (m_responseFile)
        m_responseFile->close();

    delete m_responseContent;
    delete m_responseFile;
    m_responseFile    = File;
    m_responseContent = NULL;
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

void TorcHTTPRequest::Respond(QTcpSocket *Socket, int *Abort)
{
    if (!Socket)
        return;

    QString contenttype = ResponseTypeToString(m_responseType);

    // set the response type based upon file
    if (m_responseFile)
    {
        contenttype = TorcMime::MimeTypeForFileNameAndData(m_responseFile->fileName(), m_responseFile);
        m_responseType = HTTPResponseDefault;
    }

    QByteArray contentheader = QString("Content-Type: %1\r\n").arg(contenttype).toLatin1();

    if (m_responseType == HTTPResponseUnknown)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("'%1' not found").arg(m_fullUrl));
        m_responseStatus = HTTP_NotFound;
        contentheader    = "";
    }

    // process byte range requests
    qint64 totalsize  = m_responseContent ? m_responseContent->size() : m_responseFile ? m_responseFile->size() : 0;
    qint64 sendsize   = totalsize;
    bool multipart    = false;
    static QByteArray seperator("\r\n--STaRT\r\n");
    QList<QByteArray> partheaders;

    if (m_headers->contains("Range") && m_responseStatus == HTTP_OK)
    {
        m_ranges = StringToRanges(m_headers->value("Range"), totalsize, sendsize);

        if (m_ranges.isEmpty())
        {
            SetResponseContent(new QByteArray());
            m_responseStatus = HTTP_RequestedRangeNotSatisfiable;
        }
        else
        {
            m_responseStatus = HTTP_PartialContent;
            multipart = m_ranges.size() > 1;

            if (multipart)
            {
                QList<QPair<quint64,quint64> >::const_iterator it = m_ranges.begin();
                for ( ; it != m_ranges.end(); ++it)
                {
                    QByteArray header = seperator + contentheader + QString("Content-Range: bytes %1\r\n\r\n").arg(RangeToString((*it), totalsize)).toLatin1();
                    partheaders << header;
                    sendsize += header.size();
                }
            }
        }
    }

    // format response headers
    QScopedPointer<QByteArray> headers(new QByteArray);
    QTextStream response(headers.data());

    response << TorcHTTPRequest::ProtocolToString(m_protocol) << " " << TorcHTTPRequest::StatusToString(m_responseStatus) << "\r\n";
    response << "Date: " << QDateTime::currentDateTimeUtc().toString("d MMM yyyy hh:mm:ss 'GMT'") << "\r\n";
    response << "Server: " << TorcHTTPServer::PlatformName() << ", Torc " << TORC_SOURCE_VERSION << "\r\n";
    response << "Connection: " << (m_keepAlive ? QString("keep-alive") : QString("close")) << "\r\n";
    response << "Accept-Ranges: bytes\r\n";

    if (multipart)
        response << "Content-Type: multipart/byteranges; boundary=STaRT\r\n";
    else if (m_responseType != HTTPResponseNone)
        response << contentheader;

    if (m_allowed)
        response << "Allow: " << AllowedToString(m_allowed) << "\r\n";
    response << "Content-Length: " << QString::number(sendsize) << "\r\n";

    if (m_responseStatus == HTTP_PartialContent && !multipart)
        response << "Content-Range: bytes " << RangeToString(m_ranges[0], totalsize) << "\r\n";
    else if (m_responseStatus == HTTP_RequestedRangeNotSatisfiable)
        response << "Content-Range: bytes */" << QString::number(totalsize) << "\r\n";

    if (m_responseStatus == HTTP_MovedPermanently)
        response << "Location: " << m_redirectedTo.toLatin1() << "\r\n";

    response << "\r\n";
    response.flush();

    if (*Abort)
        return;

    // send headers
    qint64 headersize = headers.data()->size();
    qint64 sent = Socket->write(headers.data()->data(), headersize);
    if (headersize != sent)
        LOG(VB_GENERAL, LOG_WARNING, QString("Buffer size %1 - but sent %2").arg(headersize).arg(sent));
    else
        LOG(VB_GENERAL, LOG_DEBUG, QString("Sent %1 header bytes").arg(sent));

    // send content
    if (!(*Abort) && m_responseContent && !m_responseContent->isEmpty() && m_requestType != HTTPHead)
    {
        if (multipart)
        {
            QList<QPair<quint64,quint64> >::const_iterator it = m_ranges.begin();
            QList<QByteArray>::const_iterator bit = partheaders.begin();
            for ( ; (it != m_ranges.end() && !(*Abort)); ++it, ++bit)
            {
                qint64 sent = Socket->write((*bit).data(), (*bit).size());
                if ((*bit).size() != sent)
                    LOG(VB_GENERAL, LOG_WARNING, QString("Buffer size %1 - but sent %2").arg((*bit).size()).arg(sent));
                else
                    LOG(VB_GENERAL, LOG_DEBUG, QString("Sent %1 multipart header bytes").arg(sent));

                if (*Abort)
                    break;

                quint64 start      = (*it).first;
                quint64 chunksize  = (*it).second - start + 1;
                sent  = Socket->write(m_responseContent->data() + start, chunksize);
                if (chunksize != sent)
                    LOG(VB_GENERAL, LOG_WARNING, QString("Buffer size %1 - but sent %2").arg(chunksize).arg(sent));
                else
                    LOG(VB_GENERAL, LOG_DEBUG, QString("Sent %1 content bytes").arg(sent));
            }
        }
        else
        {
            qint64 size = sendsize;
            qint64 offset = m_ranges.isEmpty() ? 0 : m_ranges[0].first;
            qint64 sent = Socket->write(m_responseContent->data() + offset, size);
            if (size != sent)
                LOG(VB_GENERAL, LOG_WARNING, QString("Buffer size %1 - but sent %2").arg(size).arg(sent));
            else
                LOG(VB_GENERAL, LOG_DEBUG, QString("Sent %1 content bytes").arg(sent));
        }
    }
    else if (!(*Abort) && m_responseFile && m_requestType != HTTPHead)
    {
        m_responseFile->open(QIODevice::ReadOnly);

        if (multipart)
        {
            QScopedPointer<QByteArray> buffer(new QByteArray(READ_CHUNK_SIZE, 0));

            QList<QPair<quint64,quint64> >::const_iterator it = m_ranges.begin();
            QList<QByteArray>::const_iterator bit = partheaders.begin();
            for ( ; (it != m_ranges.end() && !(*Abort)); ++it, ++bit)
            {
                qint64 sent = Socket->write((*bit).data(), (*bit).size());
                if ((*bit).size() != sent)
                    LOG(VB_GENERAL, LOG_WARNING, QString("Buffer size %1 - but sent %2").arg((*bit).size()).arg(sent));
                else
                    LOG(VB_GENERAL, LOG_DEBUG, QString("Sent %1 multipart header bytes").arg(sent));

                if (*Abort)
                    break;

                sent   = 0;
                qint64 offset = (*it).first;
                qint64 size   = (*it).second - offset + 1;

                m_responseFile->seek(offset);

#ifdef __linux__
// TODO add sendfile support with fallback to regular writes
#endif
                if (size > sent)
                {
                    do
                    {
                        qint64 remaining = qMin(size - sent, (qint64)READ_CHUNK_SIZE);
                        qint64 read = m_responseFile->read(buffer.data()->data(), remaining);
                        if (read < 0)
                        {
                            LOG(VB_GENERAL, LOG_ERR, QString("Error reading from '%1' (%2)").arg(m_responseFile->fileName()).arg(m_responseFile->errorString()));
                            break;
                        }

                        qint64 send = Socket->write(buffer.data()->data(), read);

                        if (send != read)
                        {
                            LOG(VB_GENERAL, LOG_ERR, QString("Error sending data (%1)").arg(Socket->errorString()));
                            break;
                        }

                        sent += read;
                    }
                    while (sent < size);

                    if (sent < size)
                        LOG(VB_GENERAL, LOG_ERR, QString("Failed to send all data for '%1'").arg(m_responseFile->fileName()));
                }
            }
        }
        else
        {
            qint64 sent = 0;
            qint64 size = sendsize;
            qint64 offset = m_ranges.isEmpty() ? 0 : m_ranges[0].first;

            m_responseFile->seek(offset);

#ifdef __linux__
// TODO add sendfile support with fallback to regular writes
#endif
            if (size > sent)
            {
                QScopedPointer<QByteArray> buffer(new QByteArray(READ_CHUNK_SIZE, 0));

                do
                {
                    qint64 remaining = qMin(size - sent, (qint64)READ_CHUNK_SIZE);
                    qint64 read = m_responseFile->read(buffer.data()->data(), remaining);
                    if (read < 0)
                    {
                        LOG(VB_GENERAL, LOG_ERR, QString("Error reading from '%1' (%2)").arg(m_responseFile->fileName()).arg(m_responseFile->errorString()));
                        break;
                    }

                    qint64 send = Socket->write(buffer.data()->data(), read);

                    if (send != read)
                    {
                        LOG(VB_GENERAL, LOG_ERR, QString("Error sending data (%1)").arg(Socket->errorString()));
                        break;
                    }

                    sent += read;
                }
                while (sent < size);

                if (sent < size)
                    LOG(VB_GENERAL, LOG_ERR, QString("Failed to send all data for '%1'").arg(m_responseFile->fileName()));
            }
        }

        m_responseFile->close();
    }

    Socket->flush();

    if (!m_keepAlive)
        Socket->disconnectFromHost();
}

void TorcHTTPRequest::Redirected(const QString &Redirected)
{
    m_redirectedTo = Redirected;
    m_responseStatus = HTTP_MovedPermanently;
    m_responseType   = HTTPResponseNone;
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
        case HTTP_PartialContent:      return QString("206 Partial Content");
        case HTTP_MovedPermanently:    return QString("301 Moved Permanently");
        case HTTP_BadRequest:          return QString("400 Bad Request");
        case HTTP_Unauthorized:        return QString("401 Unauthorized");
        case HTTP_Forbidden:           return QString("403 Forbidden");
        case HTTP_NotFound:            return QString("404 Not Found");
        case HTTP_MethodNotAllowed:    return QString("405 Method Not Allowed");
        case HTTP_RequestedRangeNotSatisfiable: return QString("416 Requested Range Not Satisfiable");
        case HTTP_InternalServerError: return QString("500 Internal Server Error");
    }

    return QString("Error");
}

QString TorcHTTPRequest::ResponseTypeToString(HTTPResponseType Response)
{
    switch (Response)
    {
        case HTTPResponseNone:             return QString("");
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

QString TorcHTTPRequest::AllowedToString(int Allowed)
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

int TorcHTTPRequest::StringToAllowed(const QString &Allowed)
{
    int allowed = 0;

    if (Allowed.contains("HEAD",    Qt::CaseInsensitive)) allowed += HTTPHead;
    if (Allowed.contains("GET",     Qt::CaseInsensitive)) allowed += HTTPGet;
    if (Allowed.contains("POST",    Qt::CaseInsensitive)) allowed += HTTPPost;
    if (Allowed.contains("PUT",     Qt::CaseInsensitive)) allowed += HTTPPut;
    if (Allowed.contains("DELETE",  Qt::CaseInsensitive)) allowed += HTTPDelete;
    if (Allowed.contains("OPTIONS", Qt::CaseInsensitive)) allowed += HTTPOptions;

    return allowed;
}

QList<QPair<quint64,quint64> > TorcHTTPRequest::StringToRanges(const QString &Ranges, qint64 Size, qint64 &SizeToSend)
{
    qint64 tosend = 0;
    QList<QPair<quint64,quint64> > results;
    if (Size < 1)
        return results;

    if (Ranges.contains("bytes", Qt::CaseInsensitive))
    {
        QStringList newranges = Ranges.split("=");
        if (newranges.size() == 2)
        {
            QStringList rangelist = newranges[1].split(",", QString::SkipEmptyParts);

            foreach (QString range, rangelist)
            {
                QStringList parts = range.split("-");

                if (parts.size() != 2)
                    continue;

                quint64 start = 0;
                quint64 end = 0;
                bool ok = false;
                bool havefirst  = !parts[0].trimmed().isEmpty();
                bool havesecond = !parts[1].trimmed().isEmpty();

                if (havefirst && havesecond)
                {
                    start = parts[0].toULongLong(&ok);
                    if (ok)
                    {
                        // invalid per spec
                        if (start >= Size)
                            continue;

                        end = parts[1].toULongLong(&ok);

                        // invalid per spec
                        if (end < start)
                            continue;
                    }
                }
                else if (havefirst && !havesecond)
                {
                    start = parts[0].toULongLong(&ok);

                    // invalid per spec
                    if (ok && start >= Size)
                        continue;

                    end = Size - 1;
                }
                else if (!havefirst && havesecond)
                {
                    end = Size -1;
                    start = parts[1].toULongLong(&ok);
                    if (ok)
                    {
                        start = Size - start;

                        // invalid per spec
                        if (start >= Size)
                            continue;
                    }
                }

                if (ok)
                {
                    results << QPair<quint64,quint64>(start, end);
                    tosend += end - start + 1;
                }
            }
        }
    }

    if (results.isEmpty())
    {
        SizeToSend = 0;
        LOG(VB_GENERAL, LOG_ERR, QString("Error parsing byte ranges ('%1')").arg(Ranges));
    }
    else
    {
        // rationalise overlapping range requests
        bool check = true;

        while (results.size() > 1 && check)
        {
            check = false;
            quint64 laststart = results[0].first;
            quint64 lastend   = results[0].second;

            for (int i = 1; i < results.size(); ++i)
            {
                quint64 thisstart = results[i].first;
                quint64 thisend   = results[i].second;

                if (thisstart > lastend)
                {
                    laststart = thisstart;
                    lastend   = thisend;
                    continue;
                }

                tosend -= (lastend - laststart + 1);
                tosend -= (thisend - thisstart + 1);
                results[i - 1].first  = qMin(laststart, thisstart);
                results[i - 1].second = qMax(lastend,   thisend);
                tosend += results[i - 1].second - results[i - 1].first + 1;
                results.removeAt(i);
                check = true;
                break;
            }
        }

        // set content size
        SizeToSend = tosend;
    }

    return results;
}

QString TorcHTTPRequest::RangeToString(const QPair<quint64, quint64> &Range, qint64 Size)
{
    return QString("%1-%2/%3").arg(Range.first).arg(Range.second).arg(Size);
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
