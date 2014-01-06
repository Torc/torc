/* Class TorcHTMLStaticContent
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
#include <QFile>

// Torc
#include "torclogging.h"
#include "torcdirectories.h"
#include "torcnetwork.h"
#include "torchttpserver.h"
#include "torchttprequest.h"
#include "torchttpservice.h"
#include "torchttpconnection.h"
#include "torchtmlstaticcontent.h"

/*! \class TorcHTMLStaticContent
 *  \brief Handles the provision of static server content such as html, css, js etc
*/

TorcHTMLStaticContent::TorcHTMLStaticContent()
  : TorcHTTPHandler(STATIC_DIRECTORY, "static")
{
    m_recursive = true;
}

void TorcHTMLStaticContent::ProcessHTTPRequest(TorcHTTPRequest *Request, TorcHTTPConnection* Connection)
{
    if (!Request)
        return;

    // handle options request
    if (Request->GetHTTPRequestType() == HTTPOptions)
    {
        Request->SetAllowed(HTTPHead | HTTPGet | HTTPOptions);
        Request->SetStatus(HTTP_OK);
        Request->SetResponseType(HTTPResponseNone);
        return;
    }

    // get the requested file subpath
    QString subpath = Request->GetUrl();

    // request for Torc configuration. This is used to add some appropriate Javascript globals
    // and translated strings

    if (subpath == STATIC_DIRECTORY + "js/torcconfiguration.js")
    {
        GetJavascriptConfiguration(Request, Connection);
        return;
    }

    // get the local path to static content
    QString path = GetTorcShareDir();
    if (path.endsWith("/"))
        path.chop(1);
    path += subpath;

    // file
    QFile *file = new QFile(path);

    // sanity checks
    if (file->exists())
    {
        if ((file->permissions() & QFile::ReadOther))
        {
            if (file->size() > 0)
            {
                Request->SetResponseFile(file);
                Request->SetStatus(HTTP_OK);
                return;
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, QString("'%1' is empty - ignoring").arg(path));
                Request->SetStatus(HTTP_NotFound);
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QString("'%1' is not readable").arg(path));
            Request->SetStatus(HTTP_Forbidden);
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to find '%1'").arg(path));
        Request->SetStatus(HTTP_NotFound);
    }

    Request->SetResponseType(HTTPResponseNone);
    delete file;
}

/*! \brief Construct a Javascript object that encapsulates Torc variables, enumerations and translated strings.
 *
 * The contents of this object are available as 'torcconfiguration.js'.
 *
 * \todo Translations are static and will need to be regenerated if the language is changed.
*/
void TorcHTMLStaticContent::GetJavascriptConfiguration(TorcHTTPRequest *Request, TorcHTTPConnection *Connection)
{
    if (!Request)
        return;

    // populate the list of static constants and translations
    static QMap<QString,QString> statics;
    static bool initialised = false;

    if (!initialised)
    {
        initialised = true;

        statics.insert("ServerApplication",       QCoreApplication::applicationName());
        statics.insert("SocketNotConnected",      QObject::tr("Not connected"));
        statics.insert("SocketConnecting",        QObject::tr("Connecting"));
        statics.insert("SocketConnected",         QObject::tr("Connected"));
        statics.insert("SocketReady",             QObject::tr("Ready"));
        statics.insert("SocketReconnectAfterMs", "10000"); // try and reconnect every 10 seconds
        statics.insert("ConnectedTo",             QObject::tr("Connected to "));
    }

    // generate this instance's authority (host + port)
    QString authority = (Connection && Connection->GetSocket()) ? TorcNetwork::IPAddressToLiteral(Connection->GetSocket()->localAddress()) : "unknown";
    authority += ":" + QString::number(TorcHTTPServer::GetPort());

    // generate dynamic variables
    QMap<QString,QString> dynamics;
    dynamics.insert("ServerAuthority", authority);
    dynamics.insert("ServicesPath", SERVICES_DIRECTORY);

    // and generate javascript
    QByteArray *result = new QByteArray();
    QTextStream stream(result);

    stream << QString("var torc = {\r\n");
    bool first = true;
    QMap<QString,QString>::iterator it = statics.begin();
    for ( ; it != statics.end(); ++it)
    {
        if (!first)
            stream << ",\r\n";
        stream << "  ";
        first = false;
        stream << it.key() << ": '" << it.value() << "'";
    }

    it = dynamics.begin();
    for ( ; it != dynamics.end(); ++it)
    {
        if (!first)
            stream << ",\r\n";
        stream << "  ";
        first = false;
        stream << it.key() << ": '" << it.value() << "'";
    }

    if (!first)
        stream << "\r\n";
    stream << QString("};\r\n\r\n");
    stream << QString("if (Object.freeze) { Object.freeze(torc); }\r\n");

    stream.flush();
    Request->SetStatus(HTTP_OK);
    Request->SetResponseType(HTTPResponseJSONJavascript);
    Request->SetResponseContent(result);
}
