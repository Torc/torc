/* Class TorcRAOPConnection
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

// Qt
#include <QStringList>
#include <QtEndian>
#include <QMutex>
#include <QFile>

// Torc
#include "torclocalcontext.h"
#include "torclogging.h"
#include "torcdirectories.h"
#include "torcraopconnection.h"

#include <openssl/pem.h>
#include <openssl/aes.h>
#include <arpa/inet.h>

#define MAXPACKETSIZE       2048
#define DEFAULTSAMPLERATE   44100
#define TIMINGREQUEST       0x52
#define TIMINGRESPONSE      0x53
#define SYNC                0x54
#define FIRSTSYNC          (0x54 | 0x80)
#define RANGERESEND         0x55
#define AUDIORESEND         0x56
#define AUDIO               0x60
#define FIRSTAUDIO         (0x60 | 0x80)

RSA *gRSA = NULL;

RSA* TorcRAOPConnection::LoadKey(void)
{
    static QMutex *lock = new QMutex();
    QMutexLocker locker(lock);

    if (gRSA)
        return gRSA;

    QString filename = GetTorcConfigDir() + "/RAOPKey.rsa";
    if (!QFile::exists(filename))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to load key - '%1' does not exist").arg(filename));
        return NULL;
    }

    FILE *rsafile = fopen(filename.toUtf8(), "rb");
    if (!rsafile)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to open key file '%1'").arg(filename));
        return NULL;
    }

    gRSA = PEM_read_RSAPrivateKey(rsafile, NULL, NULL, NULL);
    fclose(rsafile);

    if (gRSA)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Loaded RSA key from '%1'").arg(filename));
        return gRSA;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Failed to read RSA key from '%1'").arg(filename));
    return NULL;
}

enum RAOPState
{
    None        = (0 << 0),
    Challenged  = (1 << 0),
    Responded   = (1 << 1),
    Announce    = (1 << 2),
    AudioFormat = (1 << 3),
    AESKey      = (1 << 4),
    AESIV       = (1 << 5),
    Transport   = (1 << 6),
    AudioData   = (1 << 7)
};

/*! \class TorcRAOPConnection
 *  \brief RAOP stream handler
 *
 * TorcRAOPConnection initialises and negotiates an RAOP stream connection with a
 * client. The user must provide an RSA key for decrypting content in '~/.torc/RAOPKey.rsa'.
 * A number of features are currently unimplemented.
 *
 * \sa TorcRAOPDevice
 * \sa TorcRAOPBuffer
 *
 * \todo Authentication
 * \todo Volume control
 * \todo Timestamps
 * \todo DACP
 * \todo Flush TorcRAOPBuffer
 * \todo Custom key location
 * \todo Timestamp synchronisation
 * \todo Metadata (text and images)
 * \todo Event server
 * \todo RTP header
*/

class TorcRAOPConnectionPriv
{
  public:
    TorcRAOPConnectionPriv(QTcpSocket *Socket)
      : m_socket(Socket),
        m_textStream(NULL),
        m_dataSocket(NULL),
        m_clientControlSocket(NULL),
        m_clientControlPort(0),
        m_state(None),
        m_sampleRate(DEFAULTSAMPLERATE),
        m_textOutstanding(0),
        m_packetQueueLock(new QMutex()),
        m_playerReadTimeout(0),
        m_playerReadTimeoutCount(0),
        m_clientSendTimeout(0),
        m_clientSendTimoutCount(0),
        m_nextSequence(0),
        m_resendSequence(0),
        m_validTimestamp(0)
    {
    }

    ~TorcRAOPConnectionPriv()
    {
        ClearPacketQueue();
        delete m_packetQueueLock;
    }

    void ClearPacketQueue(void)
    {
        m_packetQueueLock->lock();
        QMap<uint64_t,QByteArray*>::iterator it = m_packetQueue.begin();
        for ( ; it != m_packetQueue.end(); ++it)
            delete it.value();
        m_packetQueue.clear();
        m_packetQueueLock->unlock();
    }

    QByteArray                 m_macAddress;
    QTcpSocket                *m_socket;
    QTextStream               *m_textStream;
    QUdpSocket                *m_dataSocket;
    QUdpSocket                *m_clientControlSocket;
    int                        m_clientControlPort;
    int                        m_state;
    QByteArray                 m_AESIV;
    AES_KEY                    m_aesKey;
    QList<int>                 m_audioFormat;
    int                        m_sampleRate;
    QHostAddress               m_peerAddress;
    int                        m_textOutstanding;
    QMap<QString,QString>      m_textHeaders;
    QByteArray                 m_textContent;
    QMap<uint64_t,QByteArray*> m_packetQueue;
    QMutex                    *m_packetQueueLock;
    int                        m_playerReadTimeout;
    int                        m_playerReadTimeoutCount;
    int                        m_clientSendTimeout;
    int                        m_clientSendTimoutCount;
    uint16_t                   m_nextSequence;
    uint16_t                   m_resendSequence;
    uint64_t                   m_validTimestamp;
    QMap<uint16_t, uint64_t>   m_resends;
};

static inline uint64_t FramesToMs(uint64_t Timestamp, int Samplerate)
{
    return (uint64_t)((double)Timestamp * 1000.0 / Samplerate);
}

#define IDENT (QString("%1: ").arg(m_reference))

TorcRAOPConnection::TorcRAOPConnection(QTcpSocket *Socket, int Reference, const QString &MACAddress)
  : QObject(NULL),
    m_reference(Reference),
    m_priv(new TorcRAOPConnectionPriv(Socket))
{
    QString macaddress = MACAddress;
    m_priv->m_macAddress = QByteArray::fromHex(macaddress.remove(':').toLatin1());
}

TorcRAOPConnection::~TorcRAOPConnection()
{
    LOG(VB_GENERAL, LOG_INFO, IDENT + "Destroying RAOP connection");
    Close();
    delete m_priv;
}

bool TorcRAOPConnection::Open(void)
{
    if (!LoadKey())
        return false;

    // hook up the master socket
    m_priv->m_textStream = new QTextStream(m_priv->m_socket);
    m_priv->m_textStream->setCodec("UTF-8");
    connect(m_priv->m_socket, SIGNAL(readyRead()), this, SLOT(ReadText()));

    // create the data socket
    m_priv->m_dataSocket = new QUdpSocket();
    connect(m_priv->m_dataSocket, SIGNAL(readyRead()), this, SLOT(ReadData()));

    if (!m_priv->m_dataSocket->bind())
    {
        LOG(VB_GENERAL, LOG_INFO, IDENT + "Failed to bind to data port");
        Close();
        return false;
    }

    LOG(VB_GENERAL, LOG_INFO, IDENT + QString("Created new RAOP connection: %1:%2<->%3:%4 (Data port %5)")
        .arg(m_priv->m_socket->localAddress().toString()).arg(m_priv->m_socket->localPort())
        .arg(m_priv->m_socket->peerAddress().toString()).arg(m_priv->m_socket->peerPort())
        .arg(m_priv->m_dataSocket->localPort()));

    return true;
}

void TorcRAOPConnection::Close(void)
{
    // kill timers
    if (m_priv->m_playerReadTimeout)
        killTimer(m_priv->m_playerReadTimeout);
    if (m_priv->m_clientSendTimeout)
        killTimer(m_priv->m_clientSendTimeout);
    m_priv->m_playerReadTimeout = 0;
    m_priv->m_clientSendTimeout = 0;

    // delete main socket
    if (m_priv->m_socket)
    {
        m_priv->m_socket->disconnect();
        m_priv->m_socket->close();
        m_priv->m_socket->deleteLater();
        m_priv->m_socket = NULL;
    }

    // delete textstream
    delete m_priv->m_textStream;
    m_priv->m_textStream = NULL;

    // delete data socket
    if (m_priv->m_dataSocket)
    {
        m_priv->m_dataSocket->disconnect();
        m_priv->m_dataSocket->close();
        m_priv->m_dataSocket->deleteLater();
        m_priv->m_dataSocket = NULL;
    }

    // client control socket
    if (m_priv->m_clientControlSocket)
    {
        m_priv->m_clientControlSocket->disconnect();
        m_priv->m_clientControlSocket->close();
        m_priv->m_clientControlSocket->deleteLater();
        m_priv->m_clientControlSocket = NULL;
    }
}

QTcpSocket* TorcRAOPConnection::MasterSocket(void)
{
    return m_priv->m_socket;
}

QByteArray* TorcRAOPConnection::Read(void)
{
    QMutexLocker locker(m_priv->m_packetQueueLock);

    m_priv->m_playerReadTimeoutCount = 0;

    QByteArray* result = NULL;

    if (m_priv->m_packetQueue.isEmpty())
        return result;

    QMap<uint64_t,QByteArray*>::iterator it = m_priv->m_packetQueue.begin();
    result = it.value();

    if (m_priv->m_validTimestamp && m_priv->m_validTimestamp <= it.key())
    {
        LOG(VB_GENERAL, LOG_INFO, IDENT + QString("Waiting on resend packet..."));
        return NULL;
    }

    LOG(VB_GENERAL, LOG_DEBUG, IDENT + QString("Releasing %1, left %2")
        .arg(it.key()).arg(m_priv->m_packetQueue.size()));

    m_priv->m_packetQueue.erase(it);
    return result;
}

void TorcRAOPConnection::ReadText(void)
{
    QTcpSocket *socket = (QTcpSocket*)sender();
    if (!socket || socket != m_priv->m_socket)
        return;

    if (m_priv->m_textOutstanding < 1)
    {
        m_priv->m_textHeaders.clear();
        m_priv->m_textContent.clear();

        bool first = true;
        while (socket->bytesAvailable())
        {
            QByteArray line = socket->readLine().trimmed();
            if (line.isEmpty())
                break;

            ParseHeader(line, first);
            first = false;
        }
    }

    m_priv->m_textOutstanding -= socket->bytesAvailable();
    m_priv->m_textContent.append(socket->readAll());

    if (m_priv->m_textOutstanding > 0)
        return;

    ProcessText();
}

void TorcRAOPConnection::ParseHeader(const QByteArray &Line, bool First)
{
    if (First)
    {
        m_priv->m_textHeaders.insert("option", Line.left(Line.indexOf(" ")));
        return;
    }

    int index = Line.indexOf(":");

    if (index < 1)
        return;

    QByteArray key   = Line.left(index).trimmed();
    QByteArray value = Line.mid(index + 1).trimmed();

    if (key == "Content-Length")
        m_priv->m_textOutstanding = value.toInt();

    m_priv->m_textHeaders.insert(key, value);
}

void TorcRAOPConnection::ProcessText(void)
{
    QString sequence = m_priv->m_textHeaders.value("CSeq");
    if (sequence.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, IDENT + "Invalid packet - no CSeq header");
        return;
    }

    *m_priv->m_textStream << "RTSP/1.0 200 OK\r\n";

    QString option = m_priv->m_textHeaders.value("option");
    if (option == "OPTIONS")
    {
        if (m_priv->m_textHeaders.contains("Apple-Challenge"))
        {
            LOG(VB_GENERAL, LOG_INFO, IDENT + "Challenge");
            m_priv->m_state |= Challenged;

            int tosize = RSA_size(LoadKey());
            uint8_t *to = new uint8_t[tosize];

            QByteArray challenge = QByteArray::fromBase64(m_priv->m_textHeaders.value("Apple-Challenge").toLatin1());
            int challengesize = challenge.size();
            if (challengesize != 16)
            {
                LOG(VB_GENERAL, LOG_WARNING, IDENT + QString("Decoded challenge size %1, expected 16").arg(challengesize));
                if (challengesize > 16)
                    challengesize = 16;
            }

            unsigned char from[38];
            memcpy(from, challenge.constData(), challengesize);
            int i = challengesize;
            if (m_priv->m_socket->localAddress().protocol() == QAbstractSocket::IPv4Protocol)
            {
                uint32_t ip = m_priv->m_socket->localAddress().toIPv4Address();
                ip = qToBigEndian(ip);
                memcpy(from + i, &ip, 4);
                i += 4;
            }
            else if (m_priv->m_socket->localAddress().protocol() == QAbstractSocket::IPv6Protocol)
            {
                Q_IPV6ADDR ip = m_priv->m_socket->localAddress().toIPv6Address();
                if(memcmp(&ip, "\x00\x00\x00\x00" "\x00\x00\x00\x00" "\x00\x00\xff\xff", 12) == 0)
                {
                    memcpy(from + i, &ip[12], 4);
                    i += 4;
                }
                else
                {
                    memcpy(from + i, &ip, 16);
                    i += 16;
                }
            }

            memcpy(from + i, m_priv->m_macAddress.constData(), 6);
            i += 6;

            int pad = 32 - i;
            if (pad > 0)
            {
                memset(from + i, 0, pad);
                i += pad;
            }

            LOG(VB_GENERAL, LOG_DEBUG, IDENT + QString("Full base64 response: '%1' size %2")
                .arg(QByteArray((const char *)from, i).toBase64().constData()).arg(i));

            RSA_private_encrypt(i, from, to, LoadKey(), RSA_PKCS1_PADDING);

            QByteArray base64 = QByteArray((const char *)to, tosize).toBase64();
            delete[] to;

            for (int pos = base64.size() - 1; pos > 0; pos--)
            {
                if (base64[pos] == '=')
                    base64[pos] = ' ';
                else
                    break;
            }
            *m_priv->m_textStream << "Apple-Response: " << base64.trimmed() << "\r\n";

        }
        m_priv->m_state |= Responded;
        *m_priv->m_textStream << "Public: ANNOUNCE, SETUP, RECORD, PAUSE, FLUSH, TEARDOWN, OPTIONS, GET_PARAMETER, SET_PARAMETER, POST, GET\r\n";
    }
    else if (option == "ANNOUNCE")
    {
        m_priv->m_state |= Announce;
        QList<QByteArray> lines = m_priv->m_textContent.split('\n');
        foreach (QByteArray line, lines)
        {
            if (line.startsWith("a=rsaaeskey:"))
            {
                QString key = line.mid(12).trimmed();
                QByteArray decodedkey = QByteArray::fromBase64(key.toLatin1());
                LOG(VB_GENERAL, LOG_DEBUG, QString("RSAAESKey: %1 (decoded size %2)")
                    .arg(key).arg(decodedkey.size()));

                if (LoadKey())
                {
                    int size = sizeof(char) * RSA_size(LoadKey());
                    char *decryptedkey = new char[size];
                    if (RSA_private_decrypt(decodedkey.size(),
                                            (const unsigned char *)decodedkey.constData(),
                                            (unsigned char *)decryptedkey,
                                            LoadKey(), RSA_PKCS1_OAEP_PADDING))
                    {
                        LOG(VB_GENERAL, LOG_DEBUG, IDENT + "Successfully decrypted AES key from RSA");
                        AES_set_decrypt_key((const unsigned char *)decryptedkey, 128, &m_priv->m_aesKey);
                        m_priv->m_state |= AESKey;
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_WARNING, IDENT + "Failed to decrypt AES key from RSA.");
                    }

                    delete [] decryptedkey;
                }
            }
            else if (line.startsWith("a=aesiv:"))
            {
                QString aesiv = line.mid(8).trimmed();
                m_priv->m_AESIV = QByteArray::fromBase64(aesiv.toLatin1());
                LOG(VB_GENERAL, LOG_DEBUG, IDENT + QString("AESIV: %1 (decoded size %2)")
                    .arg(aesiv).arg(m_priv->m_AESIV.size()));
                m_priv->m_state |= AESIV;
            }
            else if (line.startsWith("a=fmtp:"))
            {
                m_priv->m_audioFormat.clear();
                QString format = line.mid(7).trimmed();
                QList<QString> fmts = format.split(' ');
                foreach (QString fmt, fmts)
                    m_priv->m_audioFormat.append(fmt.toInt());

                if (m_priv->m_audioFormat.size() != 12)
                {
                    LOG(VB_GENERAL, LOG_ERR, IDENT + QString("Expected 12 audio format descriptors - got %1")
                        .arg(m_priv->m_audioFormat.size()));
                }
                else
                {
                    LOG(VB_GENERAL, LOG_INFO, IDENT + "Sending playback request");
                    m_priv->m_state |= AudioFormat;
                    m_priv->m_sampleRate = m_priv->m_audioFormat[11];
                    QString url = QString("raop://stream:%1?channels=%2&framesize=%3&samplesize=%4&historymult=%5&initialhistory=%6&kmodifier=%7")
                            .arg(m_reference)
                            .arg(m_priv->m_audioFormat[7])
                            .arg(m_priv->m_audioFormat[1])
                            .arg(m_priv->m_audioFormat[3])
                            .arg(m_priv->m_audioFormat[4])
                            .arg(m_priv->m_audioFormat[5])
                            .arg(m_priv->m_audioFormat[6]);
                    QVariantMap data;
                    data.insert("uri", url);
                    TorcEvent event = TorcEvent(Torc::PlayMedia, data);
                    gLocalContext->Notify(event);
                    if (!m_priv->m_playerReadTimeout)
                        m_priv->m_playerReadTimeout = startTimer(250);
                }
            }
        }
    }
    else if (option == "SETUP")
    {
        int controlport   = 0;
        int timingport    = 0;
        QString header    = m_priv->m_textHeaders.value("Transport");
        if (!header.isEmpty())
            m_priv->m_state |= Transport;
        QStringList items = header.split(";");
        bool events       = false;

        foreach (QString item, items)
        {
            if (item.startsWith("control_port"))
                controlport = item.mid(item.indexOf("=") + 1).trimmed().toUInt();
            else if (item.startsWith("timing_port"))
                timingport  = item.mid(item.indexOf("=") + 1).trimmed().toUInt();
            else if (item.startsWith("events"))
                events = true;
            (void)events;
        }

        LOG(VB_GENERAL, LOG_INFO, IDENT + QString("Negotiated setup with client %1 on port %2")
                .arg(m_priv->m_socket->peerAddress().toString()).arg(m_priv->m_socket->peerPort()));
        LOG(VB_GENERAL, LOG_DEBUG, IDENT + QString("control port: %1 timing port: %2")
                .arg(controlport).arg(timingport));

        if (!m_priv->m_clientSendTimeout)
            m_priv->m_clientSendTimeout = startTimer(250);

        m_priv->m_peerAddress = m_priv->m_socket->peerAddress();

        if (m_priv->m_clientControlSocket)
        {
            m_priv->m_clientControlSocket->disconnect();
            m_priv->m_clientControlSocket->close();
            delete m_priv->m_clientControlSocket;
        }

        m_priv->m_clientControlSocket = new QUdpSocket(this);
        if (!m_priv->m_clientControlSocket->bind(controlport))
            LOG(VB_GENERAL, LOG_ERR, IDENT + "Failed to bind to client control port");
        else
            LOG(VB_GENERAL, LOG_DEBUG, IDENT + QString("Bound to client control port %1").arg(controlport));

        m_priv->m_clientControlPort = controlport;
        connect(m_priv->m_clientControlSocket, SIGNAL(readyRead()), this, SLOT(ReadData()));

        *m_priv->m_textStream << "Transport: " << header;
        *m_priv->m_textStream << ";server_port=" << QString::number(m_priv->m_dataSocket->localPort()) << "\r\n";
        *m_priv->m_textStream << "Session: 1\r\n";
        *m_priv->m_textStream << "Audio-Jack-Status: connected\r\n";
    }
    else if (option == "GET_PARAMETER")
    {
        LOG(VB_GENERAL, LOG_DEBUG, IDENT + "GET_PARAMETER not handled");
    }
    else if (option == "SET_PARAMETER")
    {
        QString parameter = m_priv->m_textHeaders.value("Content-Type");

        if (parameter == "image/jpeg")
        {
            LOG(VB_GENERAL, LOG_INFO, IDENT + "Received image");
        }
        else if (parameter == "application/x-dmap-tagged")
        {
            LOG(VB_GENERAL, LOG_INFO, IDENT + "Received dmap data");
        }
        else if (parameter == "text/parameters")
        {
            int index   = m_priv->m_textContent.indexOf(":");
            QString key = m_priv->m_textContent.left(index);
            QString val = m_priv->m_textContent.mid(index + 1).trimmed();

            if (key == "volume")
            {
                float volume = (val.toFloat() + 30.0f) * 100.0f / 30.0f;
                if (volume < 0.01f)
                    volume = 0.0f;

                LOG(VB_GENERAL, LOG_INFO, IDENT + QString("Set volume request - %1").arg(volume));
            }
            else if (key == "progress")
            {
                uint64_t start   = 0;
                uint64_t current = 0;
                uint64_t end     = 0;

                QStringList items = val.split("/");
                if (items.size() == 3)
                {
                    start   = items[0].toUInt();
                    current = items[1].toUInt();
                    end     = items[2].toUInt();
                }

                qreal length  = ((double)(end - start)) / (qreal)m_priv->m_sampleRate;
                qreal pos     = ((double)(current - start)) / (qreal)m_priv->m_sampleRate;

                LOG(VB_GENERAL, LOG_INFO, IDENT + QString("Progress: %1 of %2").arg(pos).arg(length));
            }
            else
            {
                LOG(VB_GENERAL, LOG_INFO, IDENT + QString("Uknown text parameter %1=%2").arg(key).arg(val));
            }
        }
    }
    else if (option == "RECORD")
    {
    }
    else if (option == "FLUSH")
    {
        LOG(VB_GENERAL, LOG_INFO, IDENT + "Received 'FLUSH' request");
        m_priv->ClearPacketQueue();
        m_priv->m_resends.clear();
        m_priv->m_state &= ~AudioData;
    }
    else if (option == "TEARDOWN")
    {
        LOG(VB_GENERAL, LOG_INFO, IDENT + "Received 'TEARDOWN' request");
        *m_priv->m_textStream << "Connection: close\r\n";
    }
    else
    {
        LOG(VB_GENERAL, LOG_WARNING, IDENT + QString("Unknown command '%1'").arg(option));
    }

    *m_priv->m_textStream << "Server: AirTunes/130.14\r\n";
    *m_priv->m_textStream << "CSeq: " << sequence << "\r\n";
    *m_priv->m_textStream << "\r\n";
    m_priv->m_textStream->flush();
}

void TorcRAOPConnection::SendResend(quint64 Timestamp, quint16 Start, quint16 End)
{
    if (!m_priv->m_clientControlSocket)
        return;

    int16_t missed = (End < Start) ? (int16_t)(((int32_t)End + UINT16_MAX + 1) - Start) : End - Start;

    LOG(VB_GENERAL, LOG_INFO, IDENT + QString("Missed %1 packet(s): expected %2 got %3 ts:%4")
        .arg(missed).arg(Start).arg(End).arg(Timestamp));

    char req[8];
    req[0] = 0x80;
    req[1] = RANGERESEND | 0x80;
    *(uint16_t *)(req + 2) = htons(m_priv->m_resendSequence++);
    *(uint16_t *)(req + 4) = htons(Start);
    *(uint16_t *)(req + 6) = htons(missed);

    if (m_priv->m_clientControlSocket->writeDatagram(req, sizeof(req), m_priv->m_peerAddress, m_priv->m_clientControlPort) == sizeof(req))
    {
        for (uint16_t count = 0; count < missed; count++)
        {
            LOG(VB_GENERAL, LOG_DEBUG, IDENT + QString("Sent resend for %1").arg(Start + count));
            m_priv->m_resends.insert(Start + count, Timestamp);
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to send resend request.");
    }
}

void TorcRAOPConnection::timerEvent(QTimerEvent *Event)
{
    if (Event->timerId() == m_priv->m_playerReadTimeout)
    {
        m_priv->m_playerReadTimeoutCount++;
        if (m_priv->m_playerReadTimeoutCount == 20)
        {
            // timeout before the client
            LOG(VB_GENERAL, LOG_ERR, IDENT + "Waited 5 seconds for player to read - stopping");
            m_priv->m_socket->disconnectFromHost();
            m_priv->m_state = None;
        }
        else if (m_priv->m_playerReadTimeoutCount & 4)
        {
            LOG(VB_GENERAL, LOG_WARNING, IDENT + "Waited 1 second for player to read data");
        }
    }
    else if (Event->timerId() == m_priv->m_clientSendTimeout)
    {
        m_priv->m_clientSendTimoutCount++;
        if (m_priv->m_playerReadTimeoutCount == 20)
        {
            LOG(VB_GENERAL, LOG_ERR, IDENT + "No audio data from client in 5 seconds - stopping");
            m_priv->m_socket->disconnectFromHost();
            m_priv->m_state = None;
        }
        else if (m_priv->m_clientSendTimoutCount & 4)
        {
            LOG(VB_GENERAL, LOG_WARNING, IDENT + "No audio data from client in 1 second");
        }
    }
}

void TorcRAOPConnection::ReadData(void)
{
    QUdpSocket *socket = (QUdpSocket*)sender();
    if (!socket)
        return;

    m_priv->m_clientSendTimoutCount = 0;

    QByteArray buffer;
    while (socket->hasPendingDatagrams() && socket->state() == QAbstractSocket::BoundState)
    {
        qint64 size = socket->pendingDatagramSize();
        buffer.resize(size);
        if (socket->readDatagram(buffer.data(), size))
        {
            if ((uint8_t)buffer[0] != 0x80 && (uint8_t)buffer[0] != 0x90)
                continue;

            uint8_t type = (uint8_t)buffer[1];
            if (type != FIRSTSYNC && type != FIRSTAUDIO)
                type &= ~0x80;

            switch (type)
            {
                case SYNC:
                case FIRSTSYNC:
                    continue;
                case FIRSTAUDIO:
                case AUDIO:
                case AUDIORESEND:
                    break;
                case TIMINGRESPONSE:
                    continue;
                default:
                    LOG(VB_GENERAL, LOG_INFO, IDENT + QString("Unhandled packet type '0x%1'").arg(type, 0, 16));
                    continue;
            }

            // check we're ready for audio
            if (!m_priv->m_state & AESKey      || !m_priv->m_state & AESIV ||
                !m_priv->m_state & AudioFormat || !m_priv->m_state & Transport)
            {
                LOG(VB_GENERAL, LOG_WARNING, IDENT + "Not ready to process audio data");
                continue;
            }

            // process data
            int offset = 0;
            if (type == AUDIORESEND)
                offset += 4;

            uint16_t thissequence  = ntohs(*(uint16_t *)(buffer.data() + offset + 2));
            uint64_t thistimestamp = FramesToMs(ntohl(*(uint64_t*)(buffer.data() + offset + 4)), m_priv->m_sampleRate);

            if (type == AUDIO || type == FIRSTAUDIO)
            {
                if (type == FIRSTAUDIO)
                {
                    m_priv->m_nextSequence = thissequence;
                    m_priv->m_state |= AudioData;
                }

                if ((m_priv->m_state & AudioData) && thissequence != m_priv->m_nextSequence)
                    SendResend(thistimestamp, m_priv->m_nextSequence, thissequence);

                m_priv->m_nextSequence = thissequence + 1;
                m_priv->m_state |= AudioData;
            }

            // resends
            if (type == AUDIORESEND)
            {
                if (m_priv->m_resends.contains(thissequence))
                {
                    LOG(VB_GENERAL, LOG_DEBUG, IDENT + QString("Received resent packet (sequence %1)").arg(thissequence));
                    m_priv->m_resends.remove(thissequence);
                }
                else
                {
                    LOG(VB_GENERAL, LOG_WARNING, IDENT + QString("Received unknown resend -late? (sequence %2)").arg(thissequence));
                }
            }

            // set valid timestamp
            if (m_priv->m_resends.isEmpty())
            {
                m_priv->m_validTimestamp = 0;
            }
            else if (type != AUDIORESEND)
            {
                uint64_t valid = UINT64_MAX;
                QMutableMapIterator<uint16_t,uint64_t> it(m_priv->m_resends);
                while (it.hasNext())
                {
                    it.next();
                    if (thistimestamp - it.value() > 500)
                    {
                        LOG(VB_GENERAL, LOG_WARNING, IDENT + QString("Never received resend packet %1").arg(it.key()));
                        m_priv->m_resends.remove(it.key());
                    }
                    else if (it.value() < valid)
                    {
                        valid = it.value();
                    }
                }

                if (m_priv->m_resends.isEmpty())
                {
                    m_priv->m_validTimestamp = 0;
                }
                else
                {
                    m_priv->m_validTimestamp = valid;
                    LOG(VB_GENERAL, LOG_DEBUG, IDENT + QString("%1 outstanding resends, latest ts %2")
                        .arg(m_priv->m_resends.size()).arg(m_priv->m_validTimestamp));
                }
            }

            offset += 12;
            char* data = buffer.data() + offset;
            int length = buffer.size() - offset;
            if (length < 16)
                continue;

            int aeslen = length & ~0xf;
            unsigned char iv[16];
            QByteArray *decrypted = new QByteArray(MAXPACKETSIZE, 0);
            memcpy(iv, m_priv->m_AESIV.data(), sizeof(iv));
            AES_cbc_encrypt((const unsigned char*)data, (unsigned char*)(decrypted->data()),
                            aeslen, &m_priv->m_aesKey, iv, AES_DECRYPT);
            memcpy(decrypted->data() + aeslen, data + aeslen, length - aeslen);

            LOG(VB_GENERAL, LOG_DEBUG, IDENT + QString("Decoded %1 bytes, timestamp %2 sequence %3")
                .arg(length).arg(thistimestamp).arg(thissequence));

            m_priv->m_packetQueueLock->lock();
            m_priv->m_packetQueue.insert(thistimestamp, decrypted);
            m_priv->m_packetQueueLock->unlock();
        }
    }

}
