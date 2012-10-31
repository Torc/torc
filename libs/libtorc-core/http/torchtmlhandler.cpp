/* Class TorcHTMLHandler
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012
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
#include <QObject>

// Torc
#include "torchttpserver.h"
#include "torchttprequest.h"
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

TorcHTMLHandler::TorcHTMLHandler(const QString &Path)
  : TorcHTTPHandler(Path)
{
}

void TorcHTMLHandler::ProcessRequest(TorcHTTPServer *Server, TorcHTTPRequest *Request, TorcHTTPConnection *Connection)
{
    if (Request && Connection)
    {
        QString content = QString("<html><body><h1>%1</h1></body></html>").arg(QObject::tr("Torc is running!"));
        QByteArray *data = new QByteArray();
        data->append(content.toUtf8());
        Request->SetResponseContent(data);
        Request->SetResponseType(HTTPResponseHTML);
        Request->SetStatus(HTTP_OK);
    }
}
