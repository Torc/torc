#ifndef TORCNETWORK_H
#define TORCNETWORK_H

// Qt
#include <QObject>
#include <QtNetwork>

// Torc
#include "torccoreexport.h"

#define DEFAULT_MAC_ADDRESS QString("00:00:00:00:00:00")

class TORC_CORE_PUBLIC TorcNetwork : QNetworkAccessManager
{
    friend class TorcNetworkObject;

    Q_OBJECT

  public:
    static bool IsAvailable         (void);
    static QString GetMACAddress    (void);

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

  protected:
    TorcNetwork();
    bool    IsOnline                (void);
    bool    IsAllowed               (void);
    void    SetAllowed              (bool Allow);
    bool    event                   (QEvent *Event);
    void    CloseConnections        (void);
    void    UpdateConfiguration     (bool Creating = false);
    QString MACAddress              (void);

  private:
    bool                             m_online;
    bool                             m_allow;
    QNetworkConfigurationManager    *m_manager;
    QNetworkConfiguration            m_configuration;
    QNetworkInterface                m_interface;
};

extern TORC_CORE_PUBLIC TorcNetwork *gNetwork;
extern TORC_CORE_PUBLIC QMutex      *gNetworkLock;

#endif // TORCNETWORK_H
