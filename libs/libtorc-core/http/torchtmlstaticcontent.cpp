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
#include "torchttpserver.h"
#include "torchttprequest.h"
#include "torchttpservice.h"
#include "torchtmlstaticcontent.h"

/*! \class TorcHTMLStaticContent
 *  \brief Handles the provision of static server content such as html, css, js etc
*/

TorcHTMLStaticContent::TorcHTMLStaticContent()
  : TorcHTTPHandler("/html", "static")
{
    m_recursive = true;
}

void TorcHTMLStaticContent::ProcessHTTPRequest(TorcHTTPRequest *Request, TorcHTTPConnection*)
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

    // get the required path
    QString path = GetTorcShareDir();
    if (path.endsWith("/"))
        path.chop(1);
    path += Request->GetUrl();

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
