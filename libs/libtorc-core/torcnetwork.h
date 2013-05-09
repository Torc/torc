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

class TorcNetworkRequest : public TorcReferenceCounter
{
    friend class TorcNetwork;

  public:
    TorcNetworkRequest(const QNetworkRequest Request, int BufferSize, int *Abort);

    int             Read            (char* Buffer, qint32 BufferSize, int Timeout);
    QByteArray      ReadAll         (int Timeout);
    int             BytesAvailable  (void);

  protected:
    virtual ~TorcNetworkRequest();
    void            Write           (QNetworkReply *Reply);

  protected:
    int            *m_abort;
    bool            m_ready;
    bool            m_finished;
    QAtomicInt      m_readPosition;
    QAtomicInt      m_writePosition;
    int             m_bufferSize;
    QByteArray     *m_buffer;
    QNetworkRequest m_request;
    TorcTimer      *m_timer;
};

Q_DECLARE_METATYPE(TorcNetworkRequest*);

class TORC_CORE_PUBLIC TorcNetwork : QNetworkAccessManager
{
    friend class TorcNetworkObject;

    Q_OBJECT

  public:
    static bool IsAvailable         (void);
    static bool IsAllowed           (void);
    static QString GetMACAddress    (void);
    static bool Get                 (TorcNetworkRequest* Request);
    static void Cancel              (TorcNetworkRequest* Request);

  protected:
    static void Setup               (bool Create);

  public:
    virtual ~TorcNetwork();

  public slots:
    void    ConfigurationAdded      (const QNetworkConfiguration &Config);
    void    ConfigurationChanged    (const QNetworkConfiguration &Config);
    void    ConfigurationRemoved    (const QNetworkConfiguration &Config);
    void    OnlineStateChanged      (bool  Online);
    void    UpdateCompleted         (void);
    void    ReadyRead               (void);
    void    Finished                (void);

  protected slots:
    void    SetAllowed              (bool Allow);
    void    GetSafe                 (TorcNetworkRequest* Request);
    void    CancelSafe              (TorcNetworkRequest* Request);

  protected:
    TorcNetwork();
    bool    IsOnline                (void);
    bool    IsAllowedPriv           (void);

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
};

extern TORC_CORE_PUBLIC TorcNetwork *gNetwork;
extern TORC_CORE_PUBLIC QMutex      *gNetworkLock;

#endif // TORCNETWORK_H
