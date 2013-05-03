/* Class TorcBonjour
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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

// Std
#include <stdlib.h>
#include <arpa/inet.h>

// DNS Service Discovery
#include <dns_sd.h>

// Qt
#include <QCoreApplication>
#include <QSocketNotifier>
#include <QtEndian>
#include <QMutex>
#include <QMap>

// Torc
#include "torclogging.h"
#include "torcevent.h"
#include "torclocalcontext.h"
#include "torcadminthread.h"
#include "torcnetwork.h"
#include "torchttpserver.h"
#include "torcbonjour.h"

TorcBonjour* gBonjour = NULL;
QMutex*      gBonjourLock = new QMutex(QMutex::Recursive);

void DNSSD_API BonjourRegisterCallback(DNSServiceRef       Ref,
                                       DNSServiceFlags     Flags,
                                       DNSServiceErrorType Errorcode,
                                       const char *Name, const char *Type,
                                       const char *Domain, void *Object);
void DNSSD_API BonjourBrowseCallback(DNSServiceRef        Ref,
                                     DNSServiceFlags      Flags,
                                     uint32_t             InterfaceIndex,
                                     DNSServiceErrorType  ErrorCode,
                                     const char          *Name,
                                     const char          *Type,
                                     const char          *Domain,
                                     void                *Object);
void DNSSD_API BonjourResolveCallback(DNSServiceRef        Ref,
                                      DNSServiceFlags      Flags,
                                      uint32_t             InterfaceIndex,
                                      DNSServiceErrorType  ErrorCode,
                                      const char          *Fullname,
                                      const char          *HostTarget,
                                      uint16_t             Port,
                                      uint16_t             TxtLen,
                                      const unsigned char *TxtRecord,
                                      void                *Object);

class TorcBonjourService
{
  public:
    enum ServiceType
    {
        Service,
        Browse,
        Resolve
    };

  public:
    TorcBonjourService()
      : m_serviceType(Service),
        m_dnssRef(NULL),
        m_name(QByteArray()),
        m_type(QByteArray()),
        m_txt(QByteArray()),
        m_domain(QByteArray()),
        m_interfaceIndex(0),
        m_host(QByteArray()),
        m_port(0),
        m_lookupID(-1),
        m_fd(-1),
        m_socketNotifier(NULL)
    {
    }

    TorcBonjourService(ServiceType BonjourType, DNSServiceRef DNSSRef, const QByteArray &Name, const QByteArray &Type)
      : m_serviceType(BonjourType),
        m_dnssRef(DNSSRef),
        m_name(Name),
        m_type(Type),
        m_txt(QByteArray()),
        m_domain(QByteArray()),
        m_interfaceIndex(0),
        m_host(QByteArray()),
        m_port(0),
        m_lookupID(-1),
        m_fd(-1),
        m_socketNotifier(NULL)
    {
    }

    TorcBonjourService(ServiceType BonjourType,
                       const QByteArray &Name, const QByteArray &Type,
                       const QByteArray &Domain, uint32_t InterfaceIndex)
      : m_serviceType(BonjourType),
        m_dnssRef(NULL),
        m_name(Name),
        m_type(Type),
        m_txt(QByteArray()),
        m_domain(Domain),
        m_interfaceIndex(InterfaceIndex),
        m_host(QByteArray()),
        m_port(0),
        m_lookupID(-1),
        m_fd(-1),
        m_socketNotifier(NULL)
    {
    }

    void SetFileDescriptor(int FileDescriptor, QObject *Object)
    {
        if (FileDescriptor != -1 && Object)
        {
            m_fd = FileDescriptor;
            m_socketNotifier = new QSocketNotifier(FileDescriptor, QSocketNotifier::Read, Object);
            m_socketNotifier->setEnabled(true);
            QObject::connect(m_socketNotifier, SIGNAL(activated(int)),
                             Object, SLOT(socketReadyRead(int)));
        }
    }

    bool IsResolved(void)
    {
        return m_port && !m_ipAddresses.isEmpty();
    }

    void Deregister(void)
    {
        if (m_socketNotifier)
            m_socketNotifier->setEnabled(false);

        if (m_lookupID != -1)
            LOG(VB_NETWORK, LOG_WARNING, "Host lookup is not finished");

        if (m_dnssRef)
        {
            // Unregister
            if (m_serviceType == Browse)
            {
                LOG(VB_NETWORK, LOG_INFO, QString("Cancelling browse for '%1'")
                    .arg(m_type.data()));
            }
            else if (m_serviceType == Service)
            {
                LOG(VB_GENERAL, LOG_INFO,
                    QString("De-registering service '%1' on '%2'")
                    .arg(m_type.data()).arg(m_name.data()));
            }
            else if (m_serviceType == Resolve)
            {
                LOG(VB_NETWORK, LOG_INFO, QString("Cancelling resolve for '%1'")
                    .arg(m_type.data()));
            }

            DNSServiceRefDeallocate(m_dnssRef);
            m_dnssRef = NULL;
        }

        if (m_socketNotifier)
            m_socketNotifier->deleteLater();
        m_socketNotifier = NULL;
    }

    ServiceType      m_serviceType;
    DNSServiceRef    m_dnssRef;
    QByteArray       m_name;
    QByteArray       m_type;
    QByteArray       m_txt;
    QByteArray       m_domain;
    uint32_t         m_interfaceIndex;
    QByteArray       m_host;
    QList<QHostAddress> m_ipAddresses;
    int              m_port;
    int              m_lookupID;
    int              m_fd;
    QSocketNotifier *m_socketNotifier;
};

class TorcBonjourPriv
{
  public:
    TorcBonjourPriv(TorcBonjour *Parent)
      : m_parent(Parent),
        m_suspended(false),
        m_serviceLock(new QMutex(QMutex::Recursive)),
        m_discoveredLock(new QMutex(QMutex::Recursive))
    {
        setenv("AVAHI_COMPAT_NOWARN", "1", 1);
    }

    ~TorcBonjourPriv()
    {
        LOG(VB_GENERAL, LOG_INFO, "Closing Bonjour");

        // deregister any outstanding services (should be empty)
        {
            QMutexLocker locker(m_serviceLock);
            if (!m_suspended)
            {
                QMap<quint32,TorcBonjourService>::iterator it = m_services.begin();
                for (; it != m_services.end(); ++it)
                    (*it).Deregister();
            }
            m_services.clear();
        }

        // deallocate resolve queries
        {
            QMutexLocker locker(m_discoveredLock);
            QMap<quint32,TorcBonjourService>::iterator it = m_discoveredServices.begin();
            for (; it != m_discoveredServices.end(); ++it)
                (*it).Deregister();
            m_discoveredServices.clear();
        }

        // delete locks
        delete m_serviceLock;
        delete m_discoveredLock;
        m_serviceLock = NULL;
        m_discoveredLock = NULL;
    }

    bool IsSuspended(void)
    {
        return m_suspended;
    }

    void Suspend(void)
    {
        if (m_suspended)
        {
            LOG(VB_GENERAL, LOG_INFO, "Bonjour already suspended");
            return;
        }

        LOG(VB_GENERAL, LOG_INFO, "Suspending bonjour activities");

        m_suspended = true;

        {
            QMutexLocker locker(m_serviceLock);

            // close the services but retain the necessary details
            QMap<quint32,TorcBonjourService> services;
            QMap<quint32,TorcBonjourService>::iterator it = m_services.begin();
            for (; it != m_services.end(); ++it)
            {
                TorcBonjourService service((*it).m_serviceType, NULL, (*it).m_name, (*it).m_type);
                service.m_txt  = (*it).m_txt;
                service.m_port = (*it).m_port;
                (*it).Deregister();
                services.insert(it.key(), service);
            }

            m_services = services;
        }
    }

    void Resume(void)
    {
        if (!m_suspended)
        {
            LOG(VB_GENERAL, LOG_ERR, "Cannot resume - not suspended");
            return;
        }

        LOG(VB_GENERAL, LOG_INFO, "Resuming bonjour activities");

        {
            QMutexLocker locker(m_serviceLock);

            m_suspended = false;

            QMap<quint32,TorcBonjourService> services = m_services;
            m_services.clear();

            QMap<quint32,TorcBonjourService>::iterator it = services.begin();
            for (; it != services.end(); ++it)
            {
                if ((*it).m_serviceType == TorcBonjourService::Service)
                    (void)Register((*it).m_port, (*it).m_type, (*it).m_name, (*it).m_txt, it.key());
                else
                    (void)Browse((*it).m_type, it.key());
            }
        }
    }

    quint32 Register(quint16 Port, const QByteArray &Type,
                   const QByteArray &Name, const QByteArray &Txt,
                   quint32 Reference = 0)
    {
        if (m_suspended)
        {
            QMutexLocker locker(m_serviceLock);

            TorcBonjourService service(TorcBonjourService::Service, NULL, Name, Type);
            service.m_txt  = Txt;
            service.m_port = Port;
            quint32 reference = Reference;
            while (!reference || m_services.contains(reference))
                reference++;
            m_services.insert(reference, service);

            LOG(VB_GENERAL, LOG_ERR, "Bonjour service registration deferred until Bonjour resumed");
            return reference;
        }

        DNSServiceRef dnssref = NULL;
        DNSServiceErrorType result =
            DNSServiceRegister(&dnssref, 0,
                               0, (const char*)Name.data(),
                               (const char*)Type.data(),
                               NULL, 0, htons(Port), Txt.size(), (void*)Txt.data(),
                               BonjourRegisterCallback, this);

        if (kDNSServiceErr_NoError != result)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Register error: %1").arg(result));
            if (dnssref)
                DNSServiceRefDeallocate(dnssref);
        }
        else
        {
            QMutexLocker locker(m_serviceLock);
            quint32 reference = Reference;
            while (!reference || m_services.contains(reference))
                reference++;
            TorcBonjourService service(TorcBonjourService::Service, dnssref, Name, Type);
            service.m_txt  = Txt;
            service.m_port = Port;
            m_services.insert(reference, service);
            m_services[reference].SetFileDescriptor(DNSServiceRefSockFD(dnssref), m_parent);
            return reference;
        }

        LOG(VB_GENERAL, LOG_ERR, "Failed to register service.");
        return 0;
    }

    quint32 Browse(const QByteArray &Type, quint32 Reference = 0)
    {
        static QByteArray dummy("browser");

        if (m_suspended)
        {
            QMutexLocker locker(m_serviceLock);

            TorcBonjourService service(TorcBonjourService::Browse, NULL, dummy, Type);
            quint32 reference = Reference;
            while (!reference || m_services.contains(reference))
                reference++;
            m_services.insert(reference, service);

            LOG(VB_GENERAL, LOG_ERR, "Bonjour browse request deferred until Bonjour resumed");
            return reference;
        }

        DNSServiceRef dnssref = NULL;
        DNSServiceErrorType result = DNSServiceBrowse(&dnssref, 0,
                                                      kDNSServiceInterfaceIndexAny,
                                                      Type, NULL, BonjourBrowseCallback, this);
        if (kDNSServiceErr_NoError != result)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Browse error: %1").arg(result));
            if (dnssref)
                DNSServiceRefDeallocate(dnssref);
        }
        else
        {
            QMutexLocker locker(m_serviceLock);
            quint32 reference = Reference;
            while (!reference || m_services.contains(reference))
                reference++;
            TorcBonjourService service(TorcBonjourService::Browse, dnssref, dummy, Type);
            m_services.insert(reference, service);
            m_services[reference].SetFileDescriptor(DNSServiceRefSockFD(dnssref), m_parent);
            return reference;
        }

        LOG(VB_GENERAL, LOG_ERR, QString("Failed to browse for '%1'").arg(Type.data()));
        return 0;
    }

    void Deregister(quint32 Reference)
    {
        QByteArray type;

        {
            QMutexLocker locker(m_serviceLock);
            if (m_services.contains(Reference))
            {
                type = m_services[Reference].m_type;
                m_services[Reference].Deregister();
                m_services.remove(Reference);
            }
        }

        if (type.isEmpty())
        {
            LOG(VB_GENERAL, LOG_WARNING, "Trying to de-register an unknown service.");
            return;
        }

        // Remove any resolve requests associated with this type
        {
            QMutexLocker locker(m_discoveredLock);
            QMutableMapIterator<quint32,TorcBonjourService> it(m_discoveredServices);
            while (it.hasNext())
            {
                it.next();
                if (it.value().m_type == type)
                {
                    it.value().Deregister();
                    it.remove();
                }
            }
        }
    }

    void SocketReadyRead(int Socket)
    {
        {
            QMutexLocker lock(m_serviceLock);
            QMap<quint32,TorcBonjourService>::iterator it = m_services.begin();
            for ( ; it != m_services.end(); ++it)
            {
                if ((*it).m_fd == Socket)
                {
                    DNSServiceErrorType res = DNSServiceProcessResult((*it).m_dnssRef);
                    if (kDNSServiceErr_NoError != res)
                        LOG(VB_GENERAL, LOG_ERR, QString("Read Error: %1").arg(res));
                    return;
                }
            }
        }

        {
            QMutexLocker lock(m_discoveredLock);
            QMap<quint32,TorcBonjourService>::iterator it = m_discoveredServices.begin();
            for ( ; it != m_discoveredServices.end(); ++it)
            {
                if ((*it).m_fd == Socket)
                {
                    DNSServiceErrorType res = DNSServiceProcessResult((*it).m_dnssRef);
                    if (kDNSServiceErr_NoError != res)
                        LOG(VB_GENERAL, LOG_ERR, QString("Read Error: %1").arg(res));
                    return;
                }
            }
        }

        LOG(VB_GENERAL, LOG_WARNING, "Read request on unknown socket");
    }

    void AddBrowseResult(DNSServiceRef Reference,
                         const TorcBonjourService &Service)
    {
        {
            // validate against known browsers
            QMutexLocker locker(m_serviceLock);
            bool found = false;
            QMap<quint32,TorcBonjourService>::iterator it = m_services.begin();
            for ( ; it != m_services.end(); ++it)
            {
                if ((*it).m_dnssRef == Reference)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                LOG(VB_GENERAL, LOG_INFO, "Browser result for unknown browser");
                return;
            }
        }

        {
            // have we already seen this service?
            QMutexLocker locker(m_discoveredLock);
            QMap<quint32,TorcBonjourService>::iterator it = m_discoveredServices.begin();
            for( ; it != m_discoveredServices.end(); ++it)
            {
                if ((*it).m_name == Service.m_name &&
                    (*it).m_type == Service.m_type &&
                    (*it).m_domain == Service.m_domain &&
                    (*it).m_interfaceIndex == Service.m_interfaceIndex)
                {
                    LOG(VB_NETWORK, LOG_INFO, QString("Service '%1' already discovered - ignoring")
                        .arg(Service.m_name.data()));
                    return;
                }
            }

            // kick off resolve
            DNSServiceRef reference = NULL;
            DNSServiceErrorType error =
                DNSServiceResolve(&reference, 0, Service.m_interfaceIndex,
                                  Service.m_name.data(), Service.m_type.data(),
                                  Service.m_domain.data(), BonjourResolveCallback,
                                  this);
            if (error != kDNSServiceErr_NoError)
            {
                LOG(VB_GENERAL, LOG_ERR, "Service resolution call failed");
                if (reference)
                    DNSServiceRefDeallocate(reference);
            }
            else
            {
                // add it to our list
                TorcBonjourService service = Service;
                quint32 ref = 1;
                while (m_discoveredServices.contains(ref))
                    ref++;
                service.m_dnssRef = reference;
                m_discoveredServices.insert(ref, service);
                m_discoveredServices[ref].SetFileDescriptor(DNSServiceRefSockFD(reference), m_parent);
                LOG(VB_NETWORK, LOG_INFO, QString("Resolving '%1'").arg(service.m_name.data()));
            }
        }
    }

    void RemoveBrowseResult(DNSServiceRef Reference,
                            const TorcBonjourService &Service)
    {
        {
            // validate against known browsers
            QMutexLocker locker(m_serviceLock);
            bool found = false;
            QMap<quint32,TorcBonjourService>::iterator it = m_services.begin();
            for ( ; it != m_services.end(); ++it)
            {
                if ((*it).m_dnssRef == Reference)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                LOG(VB_GENERAL, LOG_INFO, "Browser result for unknown browser");
                return;
            }
        }

        {
            // validate against known services
            QMutexLocker locker(m_discoveredLock);
            QMutableMapIterator<quint32,TorcBonjourService> it(m_discoveredServices);
            while (it.hasNext())
            {
                it.next();
                if (it.value().m_type == Service.m_type &&
                    it.value().m_name == Service.m_name &&
                    it.value().m_domain == Service.m_domain &&
                    it.value().m_interfaceIndex == Service.m_interfaceIndex)
                {
                    LOG(VB_GENERAL, LOG_INFO, QString("Service '%1' on '%2' went away")
                        .arg(it.value().m_type.data())
                        .arg(it.value().m_host.isEmpty() ? it.value().m_name.data() : it.value().m_host.data()));
                    it.value().Deregister();
                    it.remove();
                }
            }
        }
    }

    void Resolve(DNSServiceRef Reference, DNSServiceErrorType ErrorType, const char *Fullname,
                 const char *HostTarget, uint16_t Port, uint16_t TxtLen,
                 const unsigned char *TxtRecord)
    {
        if (!Reference)
            return;

        if (ErrorType != kDNSServiceErr_NoError)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Resolve error '%1'").arg(ErrorType));
            return;
        }

        {
            QMutexLocker locker(m_discoveredLock);
            QMap<quint32,TorcBonjourService>::iterator it = m_discoveredServices.begin();
            for( ; it != m_discoveredServices.end(); ++it)
            {
                if ((*it).m_dnssRef == Reference)
                {
                    uint16_t port = ntohs(Port);
                    (*it).m_host = HostTarget;
                    (*it).m_port = port;
                    LOG(VB_NETWORK, LOG_INFO, QString("%1 (%2) resolved to %3:%4")
                        .arg((*it).m_name.data())
                        .arg((*it).m_type.data())
                        .arg(HostTarget).arg(port));
                    (*it).m_lookupID = QHostInfo::lookupHost(HostTarget, m_parent, SLOT(hostLookup(QHostInfo)));
                }
            }
        }
    }

    void HostLookup(QHostInfo HostInfo)
    {
        // igore if errored
        if (HostInfo.error() != QHostInfo::NoError)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Lookup failed with error '%1'")
                .arg(HostInfo.errorString()));
            return;
        }

        // search for the lookup id
        {
            QMutexLocker locker(m_discoveredLock);
            QMap<quint32,TorcBonjourService>::iterator it = m_discoveredServices.begin();
            for ( ; it != m_discoveredServices.end(); ++it)
            {
                if ((*it).m_lookupID == HostInfo.lookupId())
                {
                    (*it).m_lookupID = -1;
                    (*it).m_ipAddresses = HostInfo.addresses();
                    // TODO finally have a fully resolved service - use it
                    LOG(VB_GENERAL, LOG_INFO, QString("Service '%1' on '%2:%3' resolved to %4 address(es) on interface %5")
                        .arg((*it).m_type.data())
                        .arg((*it).m_host.data())
                        .arg((*it).m_port)
                        .arg((*it).m_ipAddresses.size())
                        .arg((*it).m_interfaceIndex));

                    foreach (QHostAddress address, (*it).m_ipAddresses)
                        LOG(VB_NETWORK, LOG_INFO, address.toString());
                }
            }
        }

    }

  private:
    TorcBonjour                     *m_parent;
    bool                             m_suspended;
    QMutex                          *m_serviceLock;
    QMap<quint32,TorcBonjourService> m_services;
    QMutex                          *m_discoveredLock;
    QMap<quint32,TorcBonjourService> m_discoveredServices;
};

void BonjourRegisterCallback(DNSServiceRef Ref, DNSServiceFlags Flags,
                     DNSServiceErrorType Errorcode,
                     const char *Name,   const char *Type,
                     const char *Domain, void *Object)
{
    (void)Ref;
    (void)Flags;

    TorcBonjourPriv *bonjour = static_cast<TorcBonjourPriv *>(Object);
    if (kDNSServiceErr_NoError != Errorcode)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Callback Error: %1").arg(Errorcode));
    }
    else if (bonjour)
    {
        LOG(VB_GENERAL, LOG_INFO,
            QString("Service registration complete: name '%1' type '%2'")
            .arg(QString::fromUtf8(Name)).arg(QString::fromUtf8(Type)));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("BonjourRegisterCallback for unknown object."));
    }
}

void BonjourBrowseCallback(DNSServiceRef Ref, DNSServiceFlags Flags,
                           uint32_t InterfaceIndex,
                           DNSServiceErrorType ErrorCode, const char *Name,
                           const char *Type, const char *Domain, void *Object)
{
    TorcBonjourPriv *bonjour = static_cast<TorcBonjourPriv *>(Object);
    if (!bonjour)
    {
        LOG(VB_GENERAL, LOG_ERR, "Received browse callback for unknown object");
        return;
    }

    if (ErrorCode != kDNSServiceErr_NoError)
    {
        LOG(VB_GENERAL, LOG_ERR, "Browse callback error");
        return;
    }

    TorcBonjourService service(TorcBonjourService::Resolve, Name, Type, Domain, InterfaceIndex);
    if (Flags & kDNSServiceFlagsAdd)
        bonjour->AddBrowseResult(Ref, service);
    else
        bonjour->RemoveBrowseResult(Ref, service);

    if (!(Flags & kDNSServiceFlagsMoreComing))
    {
        // TODO
    }
}

void BonjourResolveCallback(DNSServiceRef Ref, DNSServiceFlags Flags,
                            uint32_t InterfaceIndex, DNSServiceErrorType ErrorCode,
                            const char *Fullname, const char *HostTarget,
                            uint16_t Port, uint16_t TxtLen,
                            const unsigned char *TxtRecord, void *Object)
{
    TorcBonjourPriv *bonjour = static_cast<TorcBonjourPriv*>(Object);
    if (!bonjour)
    {
        LOG(VB_GENERAL, LOG_ERR, "Received resolve callback for unknown object");
        return;
    }

    bonjour->Resolve(Ref, ErrorCode, Fullname, HostTarget, Port, TxtLen, TxtRecord);
}

TorcBonjour* TorcBonjour::Instance(void)
{
    QMutexLocker locker(gBonjourLock);
    if (gBonjour)
        return gBonjour;

    gBonjour = new TorcBonjour();
    return gBonjour;
}

void TorcBonjour::Suspend(bool Suspend)
{
    QMutexLocker locker(gBonjourLock);
    if (gBonjour)
        gBonjour->SuspendPriv(Suspend);
}

void TorcBonjour::TearDown(void)
{
    QMutexLocker locker(gBonjourLock);
    delete gBonjour;
    gBonjour = NULL;
}

TorcBonjour::TorcBonjour()
  : QObject(NULL), m_priv(new TorcBonjourPriv(this))
{
    // listen for network enabled/disabled events
    gLocalContext->AddObserver(this);
}

TorcBonjour::~TorcBonjour()
{
    // stop listening for power events
    gLocalContext->RemoveObserver(this);

    // delete implementation
    delete m_priv;
    m_priv = NULL;
}

quint32 TorcBonjour::Register(quint16 Port, const QByteArray &Type,
                              const QByteArray &Name, const QByteArray &Txt)
{
    if (m_priv)
        return m_priv->Register(Port, Type, Name, Txt);
    return 0;
}

quint32 TorcBonjour::Browse(const QByteArray &Type)
{
    if (m_priv)
        return m_priv->Browse(Type);
    return 0;
}

void TorcBonjour::Deregister(quint32 Reference)
{
    if (m_priv && Reference)
        m_priv->Deregister(Reference);
}

void TorcBonjour::socketReadyRead(int Socket)
{
    if (m_priv)
        m_priv->SocketReadyRead(Socket);
}

void TorcBonjour::hostLookup(QHostInfo HostInfo)
{
    if (m_priv)
        m_priv->HostLookup(HostInfo);
}

bool TorcBonjour::event(QEvent *Event)
{
    if (Event->type() == TorcEvent::TorcEventType && m_priv)
    {
        TorcEvent* torcevent = dynamic_cast<TorcEvent*>(Event);
        if (torcevent)
        {
            int event = torcevent->Event();

            if (event == Torc::NetworkDisabled)
            {
                if (!m_priv->IsSuspended())
                    m_priv->Suspend();
            }
            else if (event == Torc::NetworkEnabled)
            {
                if (m_priv->IsSuspended())
                    m_priv->Resume();
            }
        }
    }

    return false;
}

void TorcBonjour::SuspendPriv(bool Suspend)
{
    if (m_priv)
    {
        if (Suspend && !m_priv->IsSuspended())
            m_priv->Suspend();
        else if (!Suspend && m_priv->IsSuspended())
            m_priv->Resume();
    }
}

static class TorcBrowserObject : public TorcAdminObject
{
  public:
    TorcBrowserObject()
      : TorcAdminObject(TORC_ADMIN_HIGH_PRIORITY /* start early and delete last to close TorcBonjour */),
        m_browserReference(0)
    {
    }

    void Create(void)
    {
        // always create the global instance
        TorcBonjour::Instance();

        // but immediately suspend if network access is disallowed
        if (!TorcNetwork::IsAllowed())
            TorcBonjour::Suspend(true);

        // start browsing early for other Torc applications
        // Torc::Client implies it is a consumer of media - may need revisiting
        if (gLocalContext && gLocalContext->FlagIsSet(Torc::Client) &&
            gLocalContext->GetSetting(TORC_CORE + "BrowseForTorc", true))
        {
            m_browserReference = TorcBonjour::Instance()->Browse("_torc._tcp.");
        }
    }

    void Destroy(void)
    {
        if (m_browserReference)
            TorcBonjour::Instance()->Deregister(m_browserReference);
        m_browserReference = 0;

        // N.B. We delete the global instance here
        TorcBonjour::TearDown();
    }

  private:
    quint32 m_browserReference;

} TorcBrowserObject;

static class TorcAnnounceObject : public TorcAdminObject
{
  public:
    TorcAnnounceObject()
      : TorcAdminObject(TORC_ADMIN_LOW_PRIORITY /* advertise last */),
        m_torcReference(0),
        m_httpReference(0)
    {
    }

    void Create(void)
    {
        Destroy();

        if (gLocalContext && gLocalContext->FlagIsSet(Torc::Server) &&
            gLocalContext->GetSetting(TORC_CORE + "AdvertiseService", true))
        {
            QByteArray dummy;
            int port = TorcHTTPServer::GetPort();
            QByteArray name(QCoreApplication::applicationName().toLatin1());
            name.append(" on ");
            name.append(QHostInfo::localHostName());
            QByteArray torcservice("_torc._tcp.");
            m_torcReference = TorcBonjour::Instance()->Register(port, torcservice, name, dummy);
            QByteArray httpservice("_http._tcp.");
            m_httpReference = TorcBonjour::Instance()->Register(port, httpservice, name, dummy);
        }
    }

    void Destroy(void)
    {
        if (m_httpReference)
            TorcBonjour::Instance()->Deregister(m_httpReference);
        if (m_torcReference)
            TorcBonjour::Instance()->Deregister(m_torcReference);
        m_httpReference = 0;
        m_torcReference = 0;
    }

  private:
    quint32 m_torcReference;
    quint32 m_httpReference;

} TorcAnnounceObject;
