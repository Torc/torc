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
 *  \brief A wrapper around QNetworkRequest
 *
 * TorcNetworkRequest acts as the intermediary between TorcNetwork and any other
 * class accessing the network.
 *
 * The requestor must create a valid QNetworkRequest, an abort signal and, if a streamed
 * download is required, provide a buffer size.
 *
 * In the case of streamed downloads, a circular buffer of the given size is created which
 * must be emptied (via Read) on a frequent basis. The readBufferSize for the associated QNetworkReply
 * is set to the same size. Hence there is matched buffering between Qt and Torc. Streamed downloads
 * are optimised for a single producer and a single consumer thread - any other usage is likely to lead
 * to tears before bedtime.
 *
 * For non-streamed requests, the complete download is copied to the internal buffer and can be
 * retrieved via ReadAll. Take care when downloading files of an unknown size.
 *
 * \todo Complete streamed support for seeking and peeking
 * \todo Add user messaging for network errors
*/

TorcNetworkRequest::TorcNetworkRequest(const QNetworkRequest Request, int BufferSize, int *Abort)
  : m_abort(Abort),
    m_ready(false),
    m_readPosition(0),
    m_writePosition(0),
    m_bufferSize(BufferSize),
    m_buffer(QByteArray(BufferSize, 0)),
    m_readSize(DEFAULT_STREAMED_READ_SIZE),
    m_request(Request),
    m_reply(NULL),
    m_readTimer(new TorcTimer()),
    m_writeTimer(new TorcTimer()),
    m_replyFinished(false),
    m_replyBytesAvailable(0),
    m_bytesReceived(0),
    m_bytesTotal(0)
{
}

TorcNetworkRequest::~TorcNetworkRequest()
{
    delete m_readTimer;
    delete m_writeTimer;
    m_readTimer = NULL;
    m_writeTimer = NULL;
}

int TorcNetworkRequest::BytesAvailable(void)
{
    if (!m_bufferSize)
        return m_buffer.size();

    return m_available.fetchAndAddOrdered(0);
}

int TorcNetworkRequest::Read(char *Buffer, qint32 BufferSize, int Timeout)
{
    if (!Buffer || !m_bufferSize)
        return -1;

    m_readTimer->Restart();

    while ((BytesAvailable() < m_readSize) && !(*m_abort) && (m_readTimer->Elapsed() < Timeout) && !(m_replyFinished && m_replyBytesAvailable == 0))
    {
        if (m_reply && m_replyBytesAvailable && m_writeTimer->Elapsed() > 100)
            gNetwork->Poke(m_reply);
        TorcUSleep(50000);
    }

    if (*m_abort)
        return -ECONNABORTED;

    int available = BytesAvailable();
    if (available < 1)
    {
        if (m_replyFinished && m_replyBytesAvailable == 0 && available == 0)
        {
            LOG(VB_GENERAL, LOG_INFO, "Download complete");
            return 0;
        }

        LOG(VB_GENERAL, LOG_ERR, QString("Waited %1ms for data. Aborting").arg(Timeout));
        return -ECONNABORTED;
    }

    available = qMin(available, BufferSize);

    if (m_readPosition + available > m_bufferSize)
    {
        int rest = m_bufferSize - m_readPosition;
        memcpy(Buffer, m_buffer.data() + m_readPosition, rest);
        memcpy(Buffer + rest, m_buffer.data(), available - rest);
        m_readPosition = available - rest;
    }
    else
    {
        memcpy(Buffer, m_buffer.data() + m_readPosition, available);
        m_readPosition += available;
    }

    if (m_readPosition >= m_bufferSize)
        m_readPosition -= m_bufferSize;

    static qint64 read = 0;
    read += available;

    m_available.fetchAndAddOrdered(-available);
    return available;
}

bool TorcNetworkRequest::WritePriv(QNetworkReply *Reply, char *Buffer, int Size)
{
    if (Reply && Buffer && Size)
    {
        int read = Reply->read(Buffer, Size);

        if (read < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Error reading '%1'").arg(Reply->errorString()));
            return false;
        }

        m_writePosition += read;
        if (m_writePosition >= m_bufferSize)
            m_writePosition -= m_bufferSize;
        m_available.fetchAndAddOrdered(read);

        m_replyBytesAvailable = Reply->bytesAvailable();

        if (read == Size)
            return true;

        LOG(VB_GENERAL, LOG_ERR, QString("Expected %1 bytes, got %2").arg(Size).arg(read));
    }

    return false;
}

void TorcNetworkRequest::Write(QNetworkReply *Reply)
{
    if (!Reply)
        return;

    m_writeTimer->Restart();

    if (!m_bufferSize)
    {
        m_buffer += Reply->readAll();
        return;
    }

    qint64 available = qMin(m_bufferSize - (qint64)m_available.fetchAndAddOrdered(0), Reply->bytesAvailable());

    if (m_writePosition + available > m_bufferSize)
    {
        int rest = m_bufferSize - m_writePosition;

        if (!WritePriv(Reply, m_buffer.data() + m_writePosition, rest))
            return;
        if (!WritePriv(Reply, m_buffer.data(), available - rest))
            return;
    }
    else
    {
        WritePriv(Reply, m_buffer.data() + m_writePosition, available);
    }
}

void TorcNetworkRequest::SetReadSize(int Size)
{
    m_readSize = Size;
}

void TorcNetworkRequest::DownloadProgress(qint64 Received, qint64 Total)
{
    m_bytesReceived = Received;
    m_bytesTotal    = Total;
}

QByteArray TorcNetworkRequest::ReadAll(int Timeout)
{
    if (m_bufferSize)
        LOG(VB_GENERAL, LOG_WARNING, "ReadAll called for a streamed buffer");

    m_readTimer->Restart();

    while (!(*m_abort) && (m_readTimer->Elapsed() < Timeout) && !m_replyFinished)
        TorcUSleep(50000);

    if (*m_abort)
        return QByteArray();

    if (!m_replyFinished)
        LOG(VB_GENERAL, LOG_ERR, "Download timed out - probably incomplete");

    return m_buffer;
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

void TorcNetwork::Poke(QNetworkReply *Reply)
{
    QMutexLocker locker(gNetworkLock);

    if (gNetwork)
        QMetaObject::invokeMethod(gNetwork, "PokeSafe", Qt::AutoConnection, Q_ARG(QNetworkReply*, Reply));
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
    CloseConnections();

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

        connect(reply, SIGNAL(readyRead()), this, SLOT(ReadyRead()));
        connect(reply, SIGNAL(finished()),  this, SLOT(Finished()));
        connect(reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(DownloadProgress(qint64,qint64)));
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(Error(QNetworkReply::NetworkError)));
        m_requests.insert(reply, Request);

        Request->m_ready = true;
        Request->m_reply = reply;
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

void TorcNetwork::PokeSafe(QNetworkReply *Reply)
{
    if (m_requests.contains(Reply))
        m_requests.value(Reply)->Write(Reply);
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
        m_requests.value(reply)->m_replyFinished = true;
}

void TorcNetwork::Error(QNetworkReply::NetworkError Code)
{
    QNetworkReply *reply = dynamic_cast<QNetworkReply*>(sender());

    if (reply && m_requests.contains(reply))
        LOG(VB_GENERAL, LOG_ERR, QString("Network error '%1'").arg(reply->errorString()));
}

void TorcNetwork::DownloadProgress(qint64 Received, qint64 Total)
{
    QNetworkReply *reply = dynamic_cast<QNetworkReply*>(sender());

    if (reply && m_requests.contains(reply))
    {
        TorcNetworkRequest* request = m_requests.value(reply);
        request->DownloadProgress(Received, Total);

        // we need to set the buffer size after the download has started as Qt will ignore
        // the set value if it doesn't yet know the expected size. Not ideal...
        if (Total > 0 && request->m_bufferSize && !reply->readBufferSize())
        {
            reply->setReadBufferSize(request->m_bufferSize);
            LOG(VB_GENERAL, LOG_INFO, QString("Set read buffer size to 0x%1 bytes").arg(request->m_bufferSize, 0, 16));
        }
    }
}

void TorcNetwork::CloseConnections(void)
{
    QMap<QNetworkReply*,TorcNetworkRequest*>::iterator it = m_requests.begin();
    for ( ; it != m_requests.end(); ++it)
    {
        it.key()->abort();
        it.key()->deleteLater();
        it.value()->DownRef();
    }
    m_requests.clear();
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
        qRegisterMetaType<QNetworkReply*>();
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


