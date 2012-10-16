/* Class TorcNetwork
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

// Torc
#include "torclocalcontext.h"
#include "torclogging.h"
#include "torcadminthread.h"
#include "torcnetwork.h"

TorcNetwork* gNetwork = NULL;
QMutex*      gNetworkLock = new QMutex(QMutex::Recursive);

bool TorcNetwork::IsAvailable(void)
{
    QMutexLocker locker(gNetworkLock);

    return gNetwork ? gNetwork->IsOnline() && gNetwork->IsAllowed() : false;
}

QString TorcNetwork::GetMACAddress(void)
{
    QMutexLocker locker(gNetworkLock);

    return gNetwork ? gNetwork->MACAddress() : DEFAULT_MAC_ADDRESS;
}

void TorcNetwork::Setup(bool Create)
{
    QMutexLocker locker(gNetworkLock);

    if (gNetwork)
    {
        if (Create)
            return;

        delete gNetwork;
        gNetwork = NULL;
        return;
    }

    if (Create)
    {
        gNetwork = new TorcNetwork();
        return;
    }

    delete gNetwork;
    gNetwork = NULL;
}

QString ConfigurationTypeToString(QNetworkConfiguration::Type Type)
{
    switch (Type)
    {
        case QNetworkConfiguration::InternetAccessPoint: return QString("InternetAccessPoint");
        case QNetworkConfiguration::ServiceNetwork:      return QString("ServiceNetwork");
        case QNetworkConfiguration::UserChoice:          return QString("UserDefined");
        case QNetworkConfiguration::Invalid:             return QString("Invalid");
    }

    return QString();
}

TorcNetwork::TorcNetwork()
  : QNetworkAccessManager(this),
    m_online(false),
    m_allow(false),
    m_manager(new QNetworkConfigurationManager(this))
{
    LOG(VB_GENERAL, LOG_INFO, "Opening network access manager");

    connect(m_manager, SIGNAL(configurationAdded(const QNetworkConfiguration&)),
            this,      SLOT(ConfigurationAdded(const QNetworkConfiguration&)));
    connect(m_manager, SIGNAL(configurationChanged(const QNetworkConfiguration&)),
            this,      SLOT(ConfigurationChanged(const QNetworkConfiguration&)));
    connect(m_manager, SIGNAL(configurationRemoved(const QNetworkConfiguration&)),
            this,      SLOT(ConfigurationRemoved(const QNetworkConfiguration&)));
    connect(m_manager, SIGNAL(onlineStateChanged(bool)),
            this,      SLOT(OnlineStateChanged(bool)));
    connect(m_manager, SIGNAL(updateCompleted()),
            this,      SLOT(UpdateCompleted()));

    setConfiguration(m_manager->defaultConfiguration());

    m_allow = gLocalContext->GetSetting(TORC_CORE + "NetworkEnabled", true) &&
              gLocalContext->GetFlag(Torc::Network);
    SetAllowed(m_allow);

    UpdateConfiguration(true);

    gLocalContext->AddObserver(this);
}

TorcNetwork::~TorcNetwork()
{
    gLocalContext->RemoveObserver(this);

    if (m_manager)
        m_manager->deleteLater();
    m_manager = NULL;

    LOG(VB_GENERAL, LOG_INFO, "Closing network access manager");
}

bool TorcNetwork::IsOnline(void)
{
    return m_online;
}

bool TorcNetwork::IsAllowed(void)
{
    return networkAccessible() == Accessible;
}

void TorcNetwork::SetAllowed(bool Allow)
{
    if (Allow && !gLocalContext->GetFlag(Torc::Network))
        Allow = false;

    if (networkAccessible() == Allow)
        return;

    CloseConnections();
    setNetworkAccessible(Allow ? Accessible : NotAccessible);
    LOG(VB_GENERAL, LOG_INFO, QString("Network access %1").arg(Allow ? "allowed" : "not allowed"));
}

void TorcNetwork::ConfigurationAdded(const QNetworkConfiguration &Config)
{
    UpdateConfiguration();
}

void TorcNetwork::ConfigurationChanged(const QNetworkConfiguration &Config)
{
    UpdateConfiguration();
}

void TorcNetwork::ConfigurationRemoved(const QNetworkConfiguration &Config)
{
    UpdateConfiguration();
}

void TorcNetwork::OnlineStateChanged(bool Online)
{
    UpdateConfiguration();
}

void TorcNetwork::UpdateCompleted(void)
{
    UpdateConfiguration();
}

bool TorcNetwork::event(QEvent *Event)
{
    if (Event->type() == TorcEvent::TorcEventType)
    {
        TorcEvent* torcevent = dynamic_cast<TorcEvent*>(Event);
        if (torcevent)
        {
            int event = torcevent->Event();

            if (event == Torc::DisableNetwork)
            {
                SetAllowed(false);
                UpdateConfiguration(true);
                return true;
            }
            else if (event == Torc::EnableNetwork)
            {
                SetAllowed(true);
                UpdateConfiguration(true);
                return true;
            }
        }
    }

    return false;
}

void TorcNetwork::CloseConnections(void)
{
}

void TorcNetwork::UpdateConfiguration(bool Creating)
{
    QNetworkConfiguration configuration = m_manager->defaultConfiguration();
    bool wasonline = m_online;

    if (configuration != m_configuration || Creating)
    {
        m_configuration = configuration;

        if (m_configuration.isValid())
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Network connection (Access %1) Name: %2 Bearer: %3 Type: %4")
                .arg(m_allow ? "allowed" : "disallowed")
                .arg(configuration.name())
                .arg(configuration.bearerTypeName())
                .arg(ConfigurationTypeToString(configuration.type())));
            m_online = true;

            QNetworkInterface interface = QNetworkInterface::interfaceFromName(m_configuration.name());
            m_interface = interface;
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, "No valid network connection");
            m_online = false;
        }
    }

    if (m_online && !wasonline)
    {
        LOG(VB_GENERAL, LOG_INFO, "Network up");
        gLocalContext->NotifyEvent(Torc::NetworkAvailable);
    }
    else if (wasonline && !m_online)
    {
        LOG(VB_GENERAL, LOG_INFO, "Network down");
        CloseConnections();
        gLocalContext->NotifyEvent(Torc::NetworkUnavailable);
    }
}

QString TorcNetwork::MACAddress(void)
{
    if (m_interface.isValid())
        return m_interface.hardwareAddress();

    return DEFAULT_MAC_ADDRESS;
}

class TorcNetworkObject : TorcAdminObject
{
  public:
    TorcNetworkObject()
      : TorcAdminObject(TORC_ADMIN_CRIT_PRIORITY)
    {
    }

    void Create(void)
    {
        TorcNetwork::Setup(true);
    }

    void Destroy(void)
    {
        TorcNetwork::Setup(false);
    }
} TorcNetworkObject;

