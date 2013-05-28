/* Class TorcHTMLHandler
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
#include <QCoreApplication>
#include <QObject>

// Torc
#include "torchttpserver.h"
#include "torchttprequest.h"
#include "torchttpservice.h"
#include "torchtmlhandler.h"

/*! \class TorcHTMLHandler
 *  \brief Base HTML handler
 *
 * Serves up the top level Torc HTTP interface.
 *
 * \sa TorcHTTPServer
 * \sa TorcHTTPHandler
 * \sa TorcHTTPConnection
*/

TorcHTMLHandler::TorcHTMLHandler(const QString &Path, const QString &Name)
  : TorcHTTPHandler(Path, Name)
{
}

void TorcHTMLHandler::ProcessHTTPRequest(TorcHTTPServer*, TorcHTTPRequest *Request, TorcHTTPConnection*)
{
    if (!Request)
        return;

    // handle options request
    if (Request->GetHTTPRequestType() == HTTPOptions)
    {
        // this is the 'global' options - return everything possible
        if (Request->GetUrl() == "/*")
            Request->SetAllowed(HTTPHead | HTTPGet | HTTPPost | HTTPPut | HTTPDelete | HTTPOptions);
        else
            Request->SetAllowed(HTTPHead | HTTPGet | HTTPOptions);

        Request->SetStatus(HTTP_OK);
        Request->SetResponseType(HTTPResponseDefault);
        Request->SetResponseContent(NULL);

        return;
    }

    QByteArray *result = new QByteArray();
    QTextStream stream(result);

    stream << "<html><head><title>" << QCoreApplication::applicationName() << "</title></head>";
    stream << "<body><h1>" << QCoreApplication::applicationName() << "</h1>";
    stream << "<p><a href='" << SERVICES_DIRECTORY << "/'>" << QObject::tr("Services") << "</a>";
    stream << "</body></html>";

    Request->SetResponseContent(result);
    Request->SetResponseType(HTTPResponseHTML);
    Request->SetStatus(HTTP_OK);
}
