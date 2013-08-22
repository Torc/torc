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

// Std
#include <stdlib.h>
#include <arpa/inet.h>

// DNS Service Discovery
#include <dns_sd.h>

TorcBonjour* gBonjour = NULL;                               //!< Global TorcBonjour singleton
QMutex*      gBonjourLock = new QMutex(QMutex::Recursive);  //!< Lock around access to gBonjour

void DNSSD_API BonjourRegisterCallback (DNSServiceRef        Ref,
                                        DNSServiceFlags      Flags,
                                        DNSServiceErrorType  Errorcode,
                                        const char          *Name,
                                        const char          *Type,
                                        const char          *Domain,
                                        void                *Object);
void DNSSD_API BonjourBrowseCallback   (DNSServiceRef        Ref,
                                        DNSServiceFlags      Flags,
                                        uint32_t             InterfaceIndex,
                                        DNSServiceErrorType  ErrorCode,
                                        const char          *Name,
                                        const char          *Type,
                                        const char          *Domain,
                                        void                *Object);
void DNSSD_API BonjourResolveCallback  (DNSServiceRef        Ref,
                                        DNSServiceFlags      Flags,
                                        uint32_t             InterfaceIndex,
                                        DNSServiceErrorType  ErrorCode,
                                        const char          *Fullname,
                                        const char          *HostTarget,
                                        uint16_t             Port,
                                        uint16_t             TxtLen,
                                        const unsigned char *TxtRecord,
                                        void                *Object);

/*! \class TorcBonjourService
 *  \brief Wrapper around a DNS service reference, either advertised or discovered.
 *
 * TorcBonjourService takes ownership of both the DNSServiceRef and QSocketNotifier object - to
 * ensure resources are properly released, Deregister must be called.
 *
 * \sa TorcBonjour
*/
class TorcBonjourService
{
  public:
    enum ServiceType
    {
        Service, //!< A service being advertised by this application
        Browse,  //!< An external service which we are actively trying to discover
        Resolve  //!< Address resolution for a discovered service
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
        // the default constructor only exists to keep QMap happy - it should never be used
        LOG(VB_GENERAL, LOG_WARNING, "Invalid TorcBonjourService object");
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

    /*! \fn    TorcBonjourService::SetFileDescriptor
     *  \brief Sets the file descriptor and creates a QSocketNotifier to listen for socket events
    */
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

    /*! \fn    TorcBonjourService::IsResolved
     *  \brief Returns true when the service has been fully resolved to an IP address and port.
    */
    bool IsResolved(void)
    {
        return m_port && !m_ipAddresses.isEmpty();
    }

    /*! \fn    TorcBonjourService::Deregister
     *  \brief Release all resources associated with this service.
     */
    void Deregister(void)
    {
        if (m_socketNotifier)
            m_socketNotifier->setEnabled(false);

        if (m_lookupID != -1)
        {
            LOG(VB_NETWORK, LOG_WARNING, QString("Host lookup for '%1' is not finished - cancelling").arg(m_host.data()));
            QHostInfo::abortHostLookup(m_lookupID);
            m_lookupID = -1;
        }

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

/*! \class TorcBonjourPriv
 *  \brief Private implementation of Bonjour service registration and browsing.
 *
 * \sa TorcBonjour
 * \sa TorcBonjourService
 * \sa TorcNetworkedContext
 * \sa TorcRAOPDevice
*/
class TorcBonjourPriv
{
  public:
    TorcBonjourPriv(TorcBonjour *Parent)
      : m_parent(Parent),
        m_suspended(false),
        m_serviceLock(new QMutex(QMutex::Recursive)),
        m_discoveredLock(new QMutex(QMutex::Recursive))
    {
        // the Avahi compatability layer on *nix will spam us with warnings
        qputenv("AVAHI_COMPAT_NOWARN", "1");
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

/*! \fn    TorcBonjourPriv::Suspend
 *  \brief Suspend all Bonjour activities.
 *
 * This method will deregister all existing service announcements but retain
 * the details for each. On resumption, services are then re-registered.
 *
 * \sa Resume
*/
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

    /*! \fn    TorcBonjourPriv::Resume
     *  \brief Resume Bonjour activities.
     *
     * \sa Suspend
    */
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

/*! \fn    TorcBonjourPriv::Register
 *  \brief Register a service with Bonjour.
 *
 * Register the service described by Name, Type, Txt and Port. If Bonjour
 * activities are currently suspended, the details are saved and processed
 * when Bonjour is available - in which case a reference is still returned
 * (i.e. it does not return an error).
 *
 * \param Port      The port number the service is available on (e.g. 80).
 * \param Type      A string describing the Bonjour service type (e.g. _http._tcp).
 * \param Name      A string containing a user friendly name for the service (e.g. webserver).
 * \param Txt       A pre-formatted QByteArray containing the Txt records for this service.
 * \param Reference Used to re-create a service registration following a Resume.
 * \returns An opaque reference to be passed to Deregister when complete or 0 on error.
 *
 * \sa Deregister
 * \sa TorcBonjour::MapToTxtRecord
*/
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
            // find a unique reference
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

    /*! \fn    TorcBonjourPriv::Browse
     *  \brief Search for a service named Type
     *
     * \param   Type      A string describing the Bonjour service type (e.g. _http._tcp).
     * \param   Reference Used to re-create a Browse following a Resume.
     * \returns An opaque reference to be passed to Deregister when complete or 0 on error.
     *
     * \sa Deregister
    */
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

    /*! \fn    TorcBonjourPriv::Deregister
     *  \brief Cancel a Bonjour announcement or browse.
     *
     * \param Reference An opaque reference returned from Register or Browse.
     *
     * \sa Register
     * \sa Browse
    */
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

/*! \fn    TorcBonjourPriv::SocketReadyRead
 *  \brief Handle new socket data
 *
 * Socket notifications are passed from the TorcBonjour parent and passed to
 * the Bonjour library for handling. Any further action is triggered via callbacks
 * from Bonjour.
*/
    void SocketReadyRead(int Socket)
    {
        {
            // match Socket to an announced service
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
            // match Socket to a discovered service
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

/*! \fn    TorcBonjourPriv::AddBrowseResult
 *  \brief Handle newly discovered service.
 *
 * Services that have not been seen before are added to the list of known services
 * and a name resolution is kicked off.
*/
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

    /*! \fn    TorcBonjourPriv::RemoveBrowseResult
     *  \brief Handle removed services.
    */
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
                    QVariantMap data;
                    data.insert("name", it.value().m_name.data());
                    data.insert("type", it.value().m_type.data());
                    data.insert("txtrecords", it.value().m_txt);
                    TorcEvent event(Torc::ServiceWentAway, data);
                    gLocalContext->Notify(event);

                    LOG(VB_GENERAL, LOG_INFO, QString("Service '%1' on '%2' went away")
                        .arg(it.value().m_type.data())
                        .arg(it.value().m_host.isEmpty() ? it.value().m_name.data() : it.value().m_host.data()));
                    it.value().Deregister();
                    it.remove();
                }
            }
        }
    }

/*! \fn    TorcBonjourPriv::Resolve
 *  \brief Handle name resolution respones from Bonjour
 *
 * This function is called from the Bonjour library when a discovered service is resolved
 * to a target and port. Before we can use this target, we attempt to discover its IP addresses
 * via QHostInfo.
 *
 * \sa HostLookup
*/
    void Resolve(DNSServiceRef Reference, DNSServiceErrorType ErrorType, const char *Fullname,
                 const char *HostTarget, uint16_t Port, uint16_t TxtLen,
                 const unsigned char *TxtRecord)
    {
        if (!Reference)
            return;

        {
            QMutexLocker locker(m_discoveredLock);
            QMap<quint32,TorcBonjourService>::iterator it = m_discoveredServices.begin();
            for( ; it != m_discoveredServices.end(); ++it)
            {
                if ((*it).m_dnssRef == Reference)
                {
                    if (ErrorType != kDNSServiceErr_NoError)
                    {
                        LOG(VB_GENERAL, LOG_ERR, QString("Failed to resolve '%1' (Error %2)").arg((*it).m_name.data()).arg(ErrorType));
                        return;
                    }

                    uint16_t port = ntohs(Port);
                    (*it).m_host = HostTarget;
                    (*it).m_port = port;
                    LOG(VB_NETWORK, LOG_INFO, QString("%1 (%2) resolved to %3:%4")
                        .arg((*it).m_name.data()).arg((*it).m_type.data()).arg(HostTarget).arg(port));
                    QString name(HostTarget);
                    (*it).m_lookupID = QHostInfo::lookupHost(name, m_parent, SLOT(HostLookup(QHostInfo)));
                    (*it).m_txt      = QByteArray((const char *)TxtRecord, TxtLen);
                }
            }
        }
    }

/*! \fn    TorcBonjourPriv::HostLookup
 *  \brief Handle host lookup responses from Qt.
 *
 * If the service is fully resolved to one or more IP addresses, notify the new
 * service with a Torc::ServiceDiscovered event.
*/
    void HostLookup(const QHostInfo &HostInfo)
    {
        // search for the lookup id
        {
            QMutexLocker locker(m_discoveredLock);
            QMap<quint32,TorcBonjourService>::iterator it = m_discoveredServices.begin();
            for ( ; it != m_discoveredServices.end(); ++it)
            {
                if ((*it).m_lookupID == HostInfo.lookupId())
                {
                    (*it).m_lookupID = -1;

                    // igore if errored
                    if (HostInfo.error() != QHostInfo::NoError)
                    {
                        LOG(VB_GENERAL, LOG_ERR, QString("Lookup failed for '%1' with error '%2'").arg(HostInfo.hostName()).arg(HostInfo.errorString()));
                        return;
                    }

                    (*it).m_ipAddresses = HostInfo.addresses();
                    LOG(VB_GENERAL, LOG_INFO, QString("Service '%1' on '%2:%3' resolved to %4 address(es) on interface %5")
                        .arg((*it).m_type.data())
                        .arg((*it).m_host.data())
                        .arg((*it).m_port)
                        .arg((*it).m_ipAddresses.size())
                        .arg((*it).m_interfaceIndex));

                    QStringList addresses;

                    foreach (QHostAddress address, (*it).m_ipAddresses)
                    {
                        LOG(VB_NETWORK, LOG_INFO, address.toString());
                        addresses << address.toString();
                    }

                    if (!addresses.isEmpty())
                    {
                        QVariantMap data;
                        data.insert("name", (*it).m_name.data());
                        data.insert("type", (*it).m_type.data());
                        data.insert("port", (*it).m_port);
                        data.insert("addresses", addresses);
                        data.insert("txtrecords", (*it).m_txt);
                        data.insert("host", (*it).m_host.data());
                        TorcEvent event(Torc::ServiceDiscovered, data);
                        gLocalContext->Notify(event);
                    }
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

/*! \fn    BonjourRegisterCallback
 *  \brief Handles registration callbacks from Bonjour
*/
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

/*! \fn    BonjourBrowseCallback
 *  \brief Handles browse callbacks from Bonjour
*/
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

/*! \fn    BonjourResolveCallback
 *  \brief Handles address resolution callbacks from Bonjour
*/
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

/*! \class TorcBonjour
 *  \brief Advertises and searches for services via Bonjour (aka Zero Configuration Networking/Avahi)
 *
 * TorcBonjour returns opaque references for use by client objects. These are guaranteed to remain
 * unchanged across network outages and other suspension events. Hence clients do not need to explicitly
 * monitor network status and cancel advertisements and/or browse requests.
 *
 * \sa gBonjour
 * \sa gBonjourLock
 * \sa TorcBonjourPriv
 * \sa TorcBonjourService
 *
 * \todo Send events to clients only (as for TorcSSDP)
 * \todo Guard against or properly handle duplicate requests (registration or browse)
 * \todo Make non-static public methods thread safe (Register, Browse, Deregister)
*/

/*! \fn    TorcBonjour::Instance
 *  \brief Returns the global TorcBonjour singleton.
 *
 * If it does not exist, it will be created.
*/
TorcBonjour* TorcBonjour::Instance(void)
{
    QMutexLocker locker(gBonjourLock);
    if (gBonjour)
        return gBonjour;

    gBonjour = new TorcBonjour();
    return gBonjour;
}

/*! \fn    TorcBonjour::Suspend
 *  \brief Suspends all Bonjour activities.
*/
void TorcBonjour::Suspend(bool Suspend)
{
    QMutexLocker locker(gBonjourLock);
    if (gBonjour)
        QMetaObject::invokeMethod(gBonjour, "SuspendPriv", Qt::AutoConnection, Q_ARG(bool, Suspend));
}

/*! \fn    TorcBonjour::TearDown
 *  \brief Destroys the global TorcBonjour singleton.
*/
void TorcBonjour::TearDown(void)
{
    QMutexLocker locker(gBonjourLock);
    delete gBonjour;
    gBonjour = NULL;
}

/*! \fn    TorcBonjour::MapToTxtRecord
 *  \brief Serialises a QMap into a Bonjour Txt record format.
*/
QByteArray TorcBonjour::MapToTxtRecord(const QMap<QByteArray, QByteArray> &Map)
{
    QByteArray result;

    QMap<QByteArray,QByteArray>::const_iterator it = Map.begin();
    for ( ; it != Map.end(); ++it)
    {
        QByteArray record(1, it.key().size() + it.value().size() + 1);
        record.append(it.key() + "=" + it.value());
        result.append(record);
    }

    return result;
}

/*! \fn    TorcBonjour::TxtRecordToMap
 *  \brief Extracts a QMap from a properly formatted Bonjour Txt record
*/
QMap<QByteArray,QByteArray> TorcBonjour::TxtRecordToMap(const QByteArray &TxtRecord)
{
    QMap<QByteArray,QByteArray> result;

    int position = 0;
    while (position < TxtRecord.size())
    {
        int size = TxtRecord[position++];
        QList<QByteArray> records = TxtRecord.mid(position, size).split('=');
        position += size;

        if (records.size() == 2)
            result.insert(records[0], records[1]);
    }

    return result;
}

TorcBonjour::TorcBonjour()
  : QObject(NULL), m_priv(new TorcBonjourPriv(this))
{
    // listen for network enabled/disabled events
    gLocalContext->AddObserver(this);
}

TorcBonjour::~TorcBonjour()
{
    // stop listening for network events
    gLocalContext->RemoveObserver(this);

    // delete private implementation
    delete m_priv;
    m_priv = NULL;
}

/*! \fn    TorcBonjour::Register
 *  \brief Register a service for advertisement via Bonjour.
 *
 * Registers the service described by Name, Type and TxtRecords. The service will be advertised
 * on all known system interfaces as available on port Port.
 *
 * \returns An opaque reference to be used via Deregister on success, 0 on failure.
 *
 * \sa Deregister
*/
quint32 TorcBonjour::Register(quint16 Port, const QByteArray &Type, const QByteArray &Name,
                              const QMap<QByteArray, QByteArray> &TxtRecords)
{
    if (m_priv)
    {
        QByteArray txt = MapToTxtRecord(TxtRecords);
        return m_priv->Register(Port, Type, Name, txt);
    }

    return 0;
}

/*! \fn TorcBonjour::Browse
 *  \brief Search for a service advertised via Bonjour
 *
 * Initiates a browse request for the service named by Type. The requestor must listen
 * for the Torc::ServiceDiscovered and Torc::ServiceWentAway events.
 *
 * \returns An opaque reference to be used via Deregister on success, 0 on failure.
 *
 * \sa Deregister
*/
quint32 TorcBonjour::Browse(const QByteArray &Type)
{
    if (m_priv)
        return m_priv->Browse(Type);
    return 0;
}

/*! \fn    TorcBonjour::Deregister
 *  \brief Cancel a Bonjour service registration or browse request.
 *
 * \param Reference A TorcBonjour reference previously obtained via a call to Register
 * \sa Register
 * \sa Browse
*/
void TorcBonjour::Deregister(quint32 Reference)
{
    if (m_priv && Reference)
        m_priv->Deregister(Reference);
}

/*! \fn    TorcBonjour::socketReadyRead
 *  \brief Read from a socket
 *
 * TorcBonjourPriv does not inherit QObject hence needs TorcBonjour to handle
 * slots and events.
*/
void TorcBonjour::socketReadyRead(int Socket)
{
    if (m_priv)
        m_priv->SocketReadyRead(Socket);
}

/*! \fn    TorcBonjour::HostLookup
 *  \brief Handle a host lookup signal.
 *
 * TorcBonjourPriv does not inherit QObject hence needs TorcBonjour to handle
 * slots and events.
*/
void TorcBonjour::HostLookup(const QHostInfo &HostInfo)
{
    if (m_priv)
        m_priv->HostLookup(HostInfo);
}

/*! \fn TorcBonjour::event
 *  \brief Implements QObject::event
 *
 * Listens for TorcNetwork related events and suspends or resumes Bonjour activities
 * appropriately (although strictly speaking, this is probably not necessary as this
 * will be handled internally by Bonjour).
*/
bool TorcBonjour::event(QEvent *Event)
{
    if (Event->type() == TorcEvent::TorcEventType && m_priv)
    {
        TorcEvent* torcevent = dynamic_cast<TorcEvent*>(Event);
        if (torcevent)
        {
            int event = torcevent->GetEvent();

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

    return QObject::event(Event);
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
