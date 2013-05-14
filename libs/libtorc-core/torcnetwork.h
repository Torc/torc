#ifndef TORCNETWORK_H
#define TORCNETWORK_H

// Qt
#include <QObject>
#include <QtNetwork>
#include <QAtomicInt>

// Qt
#include <QMetaType>

// Torc
#include "torccoreexport.h"
#include "torcsetting.h"
#include "torctimer.h"

#define DEFAULT_MAC_ADDRESS QString("00:00:00:00:00:00")
#define DEFAULT_STREAMED_BUFFER_SIZE (1024 * 1024 * 10)
#define DEFAULT_STREAMED_READ_SIZE   (32768)
#define DEFAULT_MAX_REDIRECTIONS     3
#define DEFAULT_USER_AGENT           QByteArray("Wget/1.12 (linux-gnu))")

class TorcNetworkRequest : public TorcReferenceCounter
{
    friend class TorcNetwork;

  public:
    TorcNetworkRequest(const QNetworkRequest Request, QNetworkAccessManager::Operation Type, int BufferSize, int *Abort);

    int             Read              (char* Buffer, qint32 BufferSize, int Timeout);
    QByteArray      ReadAll           (int Timeout);
    int             BytesAvailable    (void);
    void            SetReadSize       (int Size);
    void            DownloadProgress  (qint64 Received, qint64 Total);
    bool            CanByteServe      (void);
    QUrl            GetFinalURL       (void);

  protected:
    virtual ~TorcNetworkRequest();
    void            Write             (QNetworkReply *Reply);

  private:
    bool            WritePriv         (QNetworkReply *Reply, char* Buffer, int Size);

  protected:
    QNetworkAccessManager::Operation m_type;
    int            *m_abort;
    bool            m_started;
    int             m_readPosition;
    int             m_writePosition;
    QAtomicInt      m_available;
    int             m_bufferSize;
    QByteArray      m_buffer;
    int             m_readSize;
    QNetworkRequest m_request;
    TorcTimer      *m_readTimer;
    TorcTimer      *m_writeTimer;

    bool            m_replyFinished;
    int             m_replyBytesAvailable;
    int             m_redirectionCount;

    int             m_httpStatus;
    qint64          m_contentLength;
    bool            m_byteServingAvailable;
    qint64          m_bytesReceived;
    qint64          m_bytesTotal;
};

Q_DECLARE_METATYPE(TorcNetworkRequest*);
Q_DECLARE_METATYPE(QNetworkReply*);

class TORC_CORE_PUBLIC TorcNetwork : QNetworkAccessManager
{
    friend class TorcNetworkObject;

    Q_OBJECT

  public:
    // Public API
    static bool IsAvailable         (void);
    static bool IsAllowed           (void);
    static QString GetMACAddress    (void);
    static bool Get                 (TorcNetworkRequest* Request);
    static bool Head                (TorcNetworkRequest* Request);
    static void Cancel              (TorcNetworkRequest* Request);
    static void Poke                (TorcNetworkRequest* Request);

  protected slots:
    // QNetworkConfigurationManager
    void    ConfigurationAdded      (const QNetworkConfiguration &Config);
    void    ConfigurationChanged    (const QNetworkConfiguration &Config);
    void    ConfigurationRemoved    (const QNetworkConfiguration &Config);
    void    OnlineStateChanged      (bool  Online);
    void    UpdateCompleted         (void);

    // QNetworkReply
    void    ReadyRead               (void);
    void    Finished                (void);
    void    Error                   (QNetworkReply::NetworkError Code);
    void    SSLErrors               (const QList<QSslError> &Errors);
    void    DownloadProgress        (qint64 Received, qint64 Total);

    // Torc
    void    SetAllowed              (bool Allow);
    void    GetSafe                 (TorcNetworkRequest* Request);
    void    CancelSafe              (TorcNetworkRequest* Request);
    void    PokeSafe                (TorcNetworkRequest* Request);

  public:
    virtual ~TorcNetwork();

  protected:
    static void Setup               (bool Create);

  protected:
    TorcNetwork();
    bool    IsOnline                (void);
    bool    IsAllowedPriv           (void);
    bool    CheckHeaders            (TorcNetworkRequest* Request, QNetworkReply *Reply);
    bool    Redirected              (TorcNetworkRequest* Request, QNetworkReply *Reply);

    void    CloseConnections        (void);
    void    UpdateConfiguration     (bool Creating = false);
    QString MACAddress              (void);

  private:
    bool                             m_online;
    TorcSettingGroup                *m_networkGroup;
    TorcSetting                     *m_networkAllowed;
    QNetworkConfigurationManager    *m_manager;
    QNetworkConfiguration            m_configuration;
    QNetworkInterface                m_interface;

    QMap<QNetworkReply*,TorcNetworkRequest*> m_requests;
    QMap<TorcNetworkRequest*,QNetworkReply*> m_reverseRequests;
};

extern TORC_CORE_PUBLIC TorcNetwork *gNetwork;
extern TORC_CORE_PUBLIC QMutex      *gNetworkLock;

#endif // TORCNETWORK_H
