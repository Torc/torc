/* Class TorcHTTPHandler
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

// Torc
#include "torchttphandler.h"

/*! \class TorcHTTPHandler
 *  \brief Base HTTP response handler class.
 *
 * A TorcHTTPHandler object will be passed TorcHTTPRequest objects to process.
 * Handlers are registered with the HTTP server using TorcHTTPServer::RegisterHandler and
 * are removed with TorcHTTPServer::DeregisterHandler. The handler's signature tells the server
 * which URL the handler is expecting to process (e.g. /events).
 *
 * Concrete subclasses must implement ProcessRequest.
 *
 * \sa TorcHTTPServer
 * \sa TorcHTTPRequest
 * \sa TorcHTTPConnection
*/

TorcHTTPHandler::TorcHTTPHandler(const QString &Signature, const QString &Name)
  : m_signature(Signature),
    m_name(Name)
{
    if (!m_signature.endsWith("/"))
        m_signature += "/";
}

TorcHTTPHandler::~TorcHTTPHandler()
{
}

QString TorcHTTPHandler::Signature(void)
{
    return m_signature;
}

QString TorcHTTPHandler::Name(void)
{
    return m_name;
}
