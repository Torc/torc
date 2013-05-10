/* Class TorcNetwork
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
#include "torclocalcontext.h"
#include "torclogging.h"
#include "torcadminthread.h"
#include "torcnetwork.h"
#include "torccoreutils.h"

#include <errno.h>

/*! \class TorcNetworkRequest
 *
 * \todo Set chunk size
 * \todo Poke download thread when buffer space becomes available
 * \todo Complete ReadAll
 * \todo Check finished handling
 * \todo Fix buffer corruption
*/

TorcNetworkRequest::TorcNetworkRequest(const QNetworkRequest Request, int BufferSize, int *Abort)
  : m_abort(Abort),
    m_ready(false),
    m_finished(false),
    m_bufferSize(BufferSize),
    m_buffer(NULL),
    m_request(Request),
    m_timer(new TorcTimer())
{
    if (m_bufferSize > 0)
        m_buffer = new QByteArray(m_bufferSize, 0);
}

TorcNetworkRequest::~TorcNetworkRequest()
{
    delete m_buffer;
    delete m_timer;
}

int TorcNetworkRequest::BytesAvailable(void)
{
    if (!m_buffer)
        return -1;

    int read  = m_readPosition.fetchAndAddOrdered(0);
    int write = m_writePosition.fetchAndAddOrdered(0);

    int available = write - read;
    if (write < read)
        available += m_bufferSize;

    return available;
}

int TorcNetworkRequest::Read(char *Buffer, qint32 BufferSize, int Timeout)
{
    if (!m_buffer || !Buffer)
        return -1;

    m_timer->Restart();

    while (BytesAvailable() < 32768 && !(*m_abort) && (m_timer->Elapsed() < Timeout) && !m_finished)
        TorcUSleep(50000);

    if (*m_abort)
        return -ECONNABORTED;

    int available = BytesAvailable();
    if (available < 1)
    {
        if (m_finished && available == 0)
            return 0;

        LOG(VB_GENERAL, LOG_ERR, QString("Waited %1ms for data. Aborting").arg(Timeout));
        return -ECONNABORTED;
    }

    available    = qMin(available, BufferSize);
    int result   = available;
    int read     = m_readPosition.fetchAndAddOrdered(0);
    char* buffer = Buffer;

    if (read + available >= m_bufferSize)
    {
        int rest = m_bufferSize - read;
        memcpy((void*)buffer, m_buffer->data() + read, rest);
        buffer += rest;
        available -= rest;
        read = 0;
    }

    if (available > 0)
    {
        memcpy((void*)buffer, m_buffer->data() + read, available);
        read += available;
    }

    m_readPosition.fetchAndStoreOrdered(read);
    return result;
}

void TorcNetworkRequest::Write(QNetworkReply *Reply)
{
    if (!Reply)
        return;

    int write = m_writePosition.fetchAndAddOrdered(0);
    int read  = m_readPosition.fetchAndAddOrdered(0);

    qint64 space = read - write;
    if (read <= write)
        space += m_bufferSize;

    space = qMin(space, Reply->bytesAvailable());

    int copied = 0;
    if (write + space >= m_bufferSize)
    {
        int rest = m_bufferSize - write;
        if ((copied = Reply->read(m_buffer->data() + write, rest)) != rest)
        {
            if (copied < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Error reading '%1'").arg(Reply->errorString()));
                return;
            }

            LOG(VB_GENERAL, LOG_ERR, QString("Expect %1 bytes, got %2").arg(rest).arg(copied));
            m_writePosition.fetchAndAddOrdered(copied);
            return;
        }

        space -= rest;
        write = 0;
    }

    if (space > 0)
    {
        if ((copied = Reply->read(m_buffer->data() + write, space)) != space)
        {
            if (copied < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Error reading '%1'").arg(Reply->errorString()));
                return;
            }

            LOG(VB_GENERAL, LOG_ERR, QString("Expect %1 bytes, got %2").arg(space).arg(copied));
        }

        write += copied;
    }

    m_writePosition.fetchAndStoreOrdered(write);
}

QByteArray TorcNetworkRequest::ReadAll(int Timeout)
{
    if (m_buffer)
    {
        LOG(VB_GENERAL, LOG_ERR, "Trying to readAll from streamed buffer");
        return QByteArray();
    }

    return QByteArray();
}

TorcNetwork* gNetwork = NULL;
QMutex*      gNetworkLock = new QMutex(QMutex::Recursive);

bool TorcNetwork::IsAvailable(void)
{
    QMutexLocker locker(gNetworkLock);

    return gNetwork ? gNetwork->IsOnline() && gNetwork->IsAllowedPriv() : false;
}

bool TorcNetwork::IsAllowed(void)
{
    QMutexLocker locker(gNetworkLock);

    return gNetwork ? gNetwork->IsAllowedPriv() : false;
}

QString TorcNetwork::GetMACAddress(void)
{
    QMutexLocker locker(gNetworkLock);

    return gNetwork ? gNetwork->MACAddress() : DEFAULT_MAC_ADDRESS;
}

bool TorcNetwork::Get(TorcNetworkRequest* Request)
{
    QMutexLocker locker(gNetworkLock);

    if (IsAvailable())
    {
        QMetaObject::invokeMethod(gNetwork, "GetSafe", Qt::AutoConnection, Q_ARG(TorcNetworkRequest*, Request));
        return true;
    }

    return false;
}

void TorcNetwork::Cancel(TorcNetworkRequest *Request)
{
    QMutexLocker locker(gNetworkLock);

    if (gNetwork)
        QMetaObject::invokeMethod(gNetwork, "CancelSafe", Qt::AutoConnection, Q_ARG(TorcNetworkRequest*, Request));
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
  : QNetworkAccessManager(),
    m_online(false),
    m_manager(new QNetworkConfigurationManager(this))
{
    LOG(VB_GENERAL, LOG_INFO, "Opening network access manager");

    m_networkGroup   = new TorcSettingGroup(gRootSetting, tr("Network"));
    m_networkAllowed = new TorcSetting(m_networkGroup, QString(TORC_CORE + "AllowNetwork"),
                                       tr("Allow network access"), TorcSetting::Checkbox,
                                       true, QVariant((bool)true));
    m_networkAllowed->SetActive(gLocalContext->FlagIsSet(Torc::Network));
    // hide the network group if there is nothing to change
    m_networkGroup->SetActive(gLocalContext->FlagIsSet(Torc::Network));

    connect(m_networkAllowed, SIGNAL(ValueChanged(bool)), this, SLOT(SetAllowed(bool)));

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
    SetAllowed(m_networkAllowed->IsActive() && m_networkAllowed->GetValue().toBool());
    UpdateConfiguration(true);
}

TorcNetwork::~TorcNetwork()
{
    // release any outstanding requests
    QMap<QNetworkReply*,TorcNetworkRequest*>::iterator it = m_requests.begin();
    for ( ; it != m_requests.end(); ++it)
    {
        it.key()->abort();
        it.key()->deleteLater();
        it.value()->DownRef();
    }
    m_requests.clear();

    // remove settings
    if (m_networkAllowed)
    {
        m_networkAllowed->Remove();
        m_networkAllowed->DownRef();
    }

    if (m_networkGroup)
    {
        m_networkGroup->Remove();
        m_networkAllowed->DownRef();
    }

    m_networkAllowed = NULL;
    m_networkGroup   = NULL;

    // delete the configuration manager
    if (m_manager)
        m_manager->deleteLater();
    m_manager = NULL;

    LOG(VB_GENERAL, LOG_INFO, "Closing network access manager");
}

bool TorcNetwork::IsOnline(void)
{
    return m_online;
}

bool TorcNetwork::IsAllowedPriv(void)
{
    if (m_networkAllowed)
        return m_networkAllowed->IsActive() && m_networkAllowed->GetValue().toBool();

    return false;
}

void TorcNetwork::SetAllowed(bool Allow)
{
    if (!Allow)
        CloseConnections();

    gLocalContext->NotifyEvent(Allow ? Torc::NetworkEnabled : Torc::NetworkDisabled);
    setNetworkAccessible(Allow ? Accessible : NotAccessible);
    LOG(VB_GENERAL, LOG_INFO, QString("Network access %1").arg(Allow ? "allowed" : "not allowed"));
}

void TorcNetwork::GetSafe(TorcNetworkRequest* Request)
{
    if (Request)
    {
        Request->UpRef();
        QNetworkReply* reply = get(Request->m_request);
        if (Request->m_bufferSize)
            reply->setReadBufferSize(Request->m_bufferSize);

        connect(reply, SIGNAL(readyRead()), this, SLOT(ReadyRead()));
        connect(reply, SIGNAL(finished()),  this, SLOT(Finished()));
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(Error(QNetworkReply::NetworkError)));
        m_requests.insert(reply, Request);
        Request->m_ready = true;
    }
}

void TorcNetwork::CancelSafe(TorcNetworkRequest *Request)
{
    if (Request)
    {
        QMap<QNetworkReply*,TorcNetworkRequest*>::iterator it = m_requests.begin();
        for ( ; it != m_requests.end(); ++it)
        {
            if (it.value() == Request)
            {
                QNetworkReply* reply = it.key();
                reply->abort();
                reply->deleteLater();
                it.value()->DownRef();
                m_requests.remove(reply);
                return;
            }
        }
    }
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

void TorcNetwork::ReadyRead(void)
{
    QNetworkReply *reply = dynamic_cast<QNetworkReply*>(sender());

    if (reply && m_requests.contains(reply))
        m_requests.value(reply)->Write(reply);
}

void TorcNetwork::Finished(void)
{
    QNetworkReply *reply = dynamic_cast<QNetworkReply*>(sender());

    if (reply && m_requests.contains(reply))
        m_requests.value(reply)->m_finished = true;
}

void TorcNetwork::Error(QNetworkReply::NetworkError Code)
{
    QNetworkReply *reply = dynamic_cast<QNetworkReply*>(sender());

    if (reply && m_requests.contains(reply))
        LOG(VB_GENERAL, LOG_ERR, QString("Network error '%1'").arg(reply->errorString()));
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
            LOG(VB_GENERAL, LOG_INFO, QString("Network Name: %1 Bearer: %2 Type: %3")
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
        gLocalContext->SendMessage(Torc::InternalMessage, Torc::Local, Torc::DefaultTimeout,
                                   tr("Network"), tr("Network available"));
    }
    else if (wasonline && !m_online)
    {
        LOG(VB_GENERAL, LOG_INFO, "Network down");
        CloseConnections();
        gLocalContext->NotifyEvent(Torc::NetworkUnavailable);
        gLocalContext->SendMessage(Torc::InternalMessage, Torc::Local, Torc::DefaultTimeout,
                                   tr("Network"), tr("Network unavailable"));
    }
}

QString TorcNetwork::MACAddress(void)
{
    if (m_interface.isValid())
        return m_interface.hardwareAddress();

    return DEFAULT_MAC_ADDRESS;
}

/*! \class TorcNetworkObject
 *  \brief A static class used to create the TorcNetwork singleton in the admin thread.
*/
class TorcNetworkObject : TorcAdminObject
{
  public:
    TorcNetworkObject()
      : TorcAdminObject(TORC_ADMIN_CRIT_PRIORITY)
    {
        qRegisterMetaType<TorcNetworkRequest*>();
    }

    void Create(void)
    {
        // always create the network object to at least monitor network state.
        // if access is disallowed, it will be made inaccessible.
        TorcNetwork::Setup(true);
    }

    void Destroy(void)
    {
        TorcNetwork::Setup(false);
    }
} TorcNetworkObject;


