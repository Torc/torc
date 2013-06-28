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

TorcPeer::TorcPeer(const QString &Uuid, int Port, const QStringList &Addresses)
    : m_uuid(Uuid),
      m_port(Port),
      m_addresses(Addresses)
{
}

TorcNetworkedContext::TorcNetworkedContext()
  : QObject(),
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

                    if (event->GetEvent() == Torc::ServiceDiscovered && !m_knownPeers.contains(uuid) &&
                        uuid != gLocalContext->GetUuid().toLatin1())
                    {
                        QStringList addresses = event->Data().value("addresses").toStringList();
                        m_knownPeers.insert(uuid, TorcPeer(uuid, event->Data().value("port").toInt(), addresses));
                        LOG(VB_GENERAL, LOG_INFO, QString("New Torc peer %1").arg(uuid.data()));
                    }
                    else if (event->GetEvent() == Torc::ServiceWentAway && m_knownPeers.contains(uuid))
                    {
                        m_knownPeers.remove(uuid);
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
      : TorcAdminObject(TORC_ADMIN_HIGH_PRIORITY + 1 /* start after network and before http server */),
        m_context(NULL)
    {
    }

    ~TorcNetworkedContextObject()
    {
        Destroy();
    }

    void Create(void)
    {
        Destroy();

        m_context = new TorcNetworkedContext();
    }

    void Destroy(void)
    {
        delete m_context;
        m_context = NULL;
    }

  private:
    TorcNetworkedContext* m_context;

} TorcNetworkedContextObject;
