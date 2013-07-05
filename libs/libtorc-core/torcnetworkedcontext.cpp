/* Class TorcNetworkedContext
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

// Torc
#include "torcadminthread.h"
#include "torcnetwork.h"
#include "torcbonjour.h"
#include "torcevent.h"
#include "torchttpserver.h"
#include "upnp/torcupnp.h"
#include "upnp/torcssdp.h"
#include "torcnetworkedcontext.h"

TorcNetworkedContext *gNetworkedContext = NULL;

TorcNetworkService::TorcNetworkService(const QString &Name, const QString &UUID, int Port, const QStringList &Addresses)
  : m_name(Name),
    m_uuid(UUID),
    m_port(Port),
    m_uiAddress(QString()),
    m_addresses(Addresses)
{
    QString port = QString::number(m_port);
    foreach (QString address, Addresses)
        m_uiAddress += address + ":" + port + " ";
}

QString TorcNetworkService::GetName(void)
{
    return m_name;
}

QString TorcNetworkService::GetUuid(void)
{
    return m_uuid;
}

int TorcNetworkService::GetPort(void)
{
    return m_port;
}

QString TorcNetworkService::GetAddress(void)
{
    return m_uiAddress;
}

QStringList TorcNetworkService::GetAddresses(void)
{
    return m_addresses;
}

TorcNetworkedContext::TorcNetworkedContext()
  : QAbstractListModel(),
    TorcObservable(),
    m_bonjourBrowserReference(0)
{
    // listen for events
    gLocalContext->AddObserver(this);

#if defined(CONFIG_LIBDNS_SD) && CONFIG_LIBDNS_SD
    // always create the global instance
    TorcBonjour::Instance();

    // but immediately suspend if network access is disallowed
    if (!TorcNetwork::IsAllowed())
        TorcBonjour::Suspend(true);
#endif

    // start browsing early for other Torc applications
    // Torc::Client implies it is a consumer of media - may need revisiting
    if (gLocalContext->FlagIsSet(Torc::Client))
    {
#if defined(CONFIG_LIBDNS_SD) && CONFIG_LIBDNS_SD
        m_bonjourBrowserReference = TorcBonjour::Instance()->Browse("_torc._tcp.");
#endif

        // NB TorcSSDP singleton isn't running yet - but the request will be queued
        TorcSSDP::Search(TORC_ROOT_UPNP_DEVICE, this);
    }

    TorcUPNPDescription upnp(QString("uuid:%1").arg(gLocalContext->GetUuid()), TORC_ROOT_UPNP_DEVICE, "LOCATION", 1000);
    TorcSSDP::Announce(upnp);
}

TorcNetworkedContext::~TorcNetworkedContext()
{
    TorcUPNPDescription upnp(QString("uuid:%1").arg(gLocalContext->GetUuid()), TORC_ROOT_UPNP_DEVICE, "LOCATION", 1000);
    TorcSSDP::CancelAnnounce(upnp);

    // stoplistening
    gLocalContext->RemoveObserver(this);

#if defined(CONFIG_LIBDNS_SD) && CONFIG_LIBDNS_SD
    // stop browsing for torc._tcp
    if (m_bonjourBrowserReference)
        TorcBonjour::Instance()->Deregister(m_bonjourBrowserReference);
    m_bonjourBrowserReference = 0;

    // cancel upnp search
    TorcSSDP::CancelSearch(TORC_ROOT_UPNP_DEVICE, this);

    // N.B. We delete the global instance here
    TorcBonjour::TearDown();
#endif

    while (!m_discoveredServices.isEmpty())
        delete m_discoveredServices.takeLast();
}

QVariant TorcNetworkedContext::data(const QModelIndex &Index, int Role) const
{
    int row = Index.row();

    if (row < 0 || row >= m_discoveredServices.size() || Role != Qt::DisplayRole)
        return QVariant();

    return QVariant::fromValue(m_discoveredServices.at(row));
}

QHash<int,QByteArray> TorcNetworkedContext::roleNames(void) const
{
    QHash<int,QByteArray> roles;
    roles.insert(Qt::DisplayRole, "m_name");
    roles.insert(Qt::DisplayRole, "m_uuid");
    roles.insert(Qt::DisplayRole, "m_port");
    roles.insert(Qt::DisplayRole, "m_uiaddress");
    return roles;
}

int TorcNetworkedContext::rowCount(const QModelIndex&) const
{
    return m_discoveredServices.size();
}

bool TorcNetworkedContext::event(QEvent *Event)
{
    if (Event->type() == TorcEvent::TorcEventType)
    {
        TorcEvent *event = static_cast<TorcEvent*>(Event);
        if (event && (event->GetEvent() == Torc::ServiceDiscovered || event->GetEvent() == Torc::ServiceWentAway))
        {
            if (event->Data().contains("txtrecords"))
            {
#if defined(CONFIG_LIBDNS_SD) && CONFIG_LIBDNS_SD
                // txtrecords is Bonjour specific
                QMap<QByteArray,QByteArray> records = TorcBonjour::TxtRecordToMap(event->Data().value("txtrecords").toByteArray());

                if (records.contains("uuid"))
                {
                    QByteArray uuid = records.value("uuid");
                    QByteArray name = event->Data().value("name").toByteArray();

                    if (event->GetEvent() == Torc::ServiceDiscovered && !m_serviceList.contains(uuid) &&
                        uuid != gLocalContext->GetUuid().toLatin1())
                    {
                        QStringList addresses = event->Data().value("addresses").toStringList();

                        int position = m_discoveredServices.size();
                        beginInsertRows(QModelIndex(), position, position);
                        m_discoveredServices.append(new TorcNetworkService(name, uuid, event->Data().value("port").toInt(), addresses));
                        endInsertRows();
                        m_serviceList.append(uuid);

                        LOG(VB_GENERAL, LOG_INFO, QString("New Torc peer %1").arg(name.data()));
                    }
                    else if (event->GetEvent() == Torc::ServiceWentAway && m_serviceList.contains(uuid))
                    {
                        (void)m_serviceList.removeAll(uuid);

                        for (int i = 0; i < m_discoveredServices.size(); ++i)
                        {
                            if (m_discoveredServices.at(i)->GetUuid() == uuid)
                            {
                                // remove the item from the model
                                beginRemoveRows(QModelIndex(), i, i);
                                delete m_discoveredServices.takeAt(i);
                                endRemoveRows();

                                break;
                            }
                        }

                        LOG(VB_GENERAL, LOG_INFO, QString("Torc peer %1 went away").arg(uuid.data()));
                    }
                }
#endif
            }
            else if (event->Data().contains("usn"))
            {
                // USN == Unique Service Name (UPnP)
                QString uuid = TorcUPNP::UUIDFromUSN(event->Data().value("usn").toString());
            }
        }
    }

    return false;
}

static class TorcNetworkedContextObject : public TorcAdminObject
{
  public:
    TorcNetworkedContextObject()
      : TorcAdminObject(TORC_ADMIN_HIGH_PRIORITY + 1 /* start after network and before http server */)
    {
    }

    ~TorcNetworkedContextObject()
    {
        Destroy();
    }

    void Create(void)
    {
        Destroy();

        gNetworkedContext = new TorcNetworkedContext();
    }

    void Destroy(void)
    {
        delete gNetworkedContext;
        gNetworkedContext = NULL;
    }

} TorcNetworkedContextObject;
