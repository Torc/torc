/* Class TorcHTMLServicesHelp
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
#include <QCoreApplication>
#include <QMetaObject>
#include <QMetaMethod>
#include <QObject>

// Torc
#include "torclocalcontext.h"
#include "torcserialiser.h"
#include "torchttpserver.h"
#include "torchttprequest.h"
#include "torchttpservice.h"
#include "torchtmlserviceshelp.h"

TorcHTMLServicesHelp::TorcHTMLServicesHelp(TorcHTTPServer *Server)
  : QObject(),
    TorcHTTPService(this, "", "services", TorcHTMLServicesHelp::staticMetaObject, QString("HandlersChanged"))
{
    connect(Server, SIGNAL(HandlersChanged()), this, SLOT(HandlersChanged()));
}

TorcHTMLServicesHelp::~TorcHTMLServicesHelp()
{
}

QString TorcHTMLServicesHelp::GetUIName(void)
{
    return tr("Services");
}

void TorcHTMLServicesHelp::ProcessHTTPRequest(TorcHTTPRequest *Request, TorcHTTPConnection* Connection)
{
    if (!Request)
        return;

    // handle own service
    if (!Request->GetMethod().isEmpty())
    {
        TorcHTTPService::ProcessHTTPRequest(Request, Connection);
        return;
    }

    // handle options request
    if (Request->GetHTTPRequestType() == HTTPOptions)
    {
        Request->SetAllowed(HTTPHead | HTTPGet | HTTPOptions);
        Request->SetStatus(HTTP_OK);
        Request->SetResponseType(HTTPResponseDefault);
        Request->SetResponseContent(NULL);
        return;
    }

    QByteArray *result = new QByteArray();
    QTextStream stream(result);

    QVariantMap services = TorcHTTPServer::GetServiceHandlers();

    stream << "<html><head><title>" << QCoreApplication::applicationName() << "</title></head>";
    stream << "<body><h1><a href='/'>" << QCoreApplication::applicationName();
    stream << "<a> " << GetUIName() << "</a></h1>";

    if (services.isEmpty())
    {
        stream << "<h3>" << QObject::tr("No services are registered") << "</h3>";
    }
    else
    {
        stream << "<h3>" << QObject::tr("Available services") << "</h3>";
        QVariantMap::iterator it = services.begin();
        for ( ; it != services.end(); ++it)
        {
            QVariantMap map = it.value().toMap();
            stream << map.value("name").toString() << " <a href='" << map.value("path").toString() + "Help" << "'>" << map.value("path").toString() << "</a><br>";
        }
    }

    stream << "<h3>" << QObject::tr("Supported return formats") << ":</h3>";
    TorcSerialiserFactory *factory = TorcSerialiserFactory::GetTorcSerialiserFactory();
    for ( ; factory; factory = factory->NextTorcSerialiserFactory())
        stream << factory->Description() << " (" << factory->Accepts() << ")</br>";

    stream << "</body></html>";
    stream.flush();
    Request->SetStatus(HTTP_OK);
    Request->SetResponseType(HTTPResponseHTML);
    Request->SetResponseContent(result);
}

void TorcHTMLServicesHelp::SubscriberDeleted(QObject *Subscriber)
{
    TorcHTTPService::HandleSubscriberDeleted(Subscriber);
}

/*! \brief Return complete application information.
 *
 * This acts as a convenience method for peers to retrieve pertinant application
 * information with one remote call.
*/
QVariantMap TorcHTMLServicesHelp::GetDetails(void)
{
    // NB keys here match those of the relevant stand alone methods. Take care not to break them.
    QVariantMap results;

    int index = TorcHTMLServicesHelp::staticMetaObject.indexOfClassInfo("Version");
    results.insert("version",   index > -1 ? TorcHTMLServicesHelp::staticMetaObject.classInfo(index).value() : "unknown");
    results.insert("services",  GetServiceList());
    results.insert("starttime", GetStartTime());
    results.insert("priority",  GetPriority());
    results.insert("uuid",      GetUuid());

    return results;
}

QVariantMap TorcHTMLServicesHelp::GetServiceList(void)
{
    return TorcHTTPServer::GetServiceHandlers();
}

qint64 TorcHTMLServicesHelp::GetStartTime(void)
{
    return gLocalContext->GetStartTime();
}

int TorcHTMLServicesHelp::GetPriority(void)
{
    return gLocalContext->GetPriority();
}

QString TorcHTMLServicesHelp::GetUuid(void)
{
    return gLocalContext->GetUuid();
}

void TorcHTMLServicesHelp::HandlersChanged(void)
{
    emit ServiceListChanged();
}
