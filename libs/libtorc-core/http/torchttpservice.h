#ifndef TORCSERVICE_H
#define TORCSERVICE_H

// Qt
#include <QMap>
#include <QMutex>
#include <QMetaObject>
#include <QCoreApplication>

// Torc
#include "torccoreexport.h"
#include "torchttphandler.h"
#include "torchttprequest.h"

class TorcHTTPServer;
class TorcHTTPConnection;
class MethodParameters;

#define SERVICES_DIRECTORY QString("/services/")

class TORC_CORE_PUBLIC TorcHTTPService : public TorcHTTPHandler
{
    Q_DECLARE_TR_FUNCTIONS(TorcHTTPService)

  public:
    TorcHTTPService(QObject *Parent, const QString &Signature, const QString &Name,
                    const QMetaObject &MetaObject, const QString &Blacklist = QString(""));
    virtual ~TorcHTTPService();

    void         ProcessHTTPRequest       (TorcHTTPRequest *Request, TorcHTTPConnection *Connection);
    QVariantMap  ProcessRequest           (const QString &Method, const QVariant &Parameters, QObject *Connection);
    QString      GetMethod                (int Index);
    QVariant     GetProperty              (int Index);

    virtual QString GetUIName             (void);

  protected:
    void         UserHelp                 (TorcHTTPRequest *Request, TorcHTTPConnection *Connection);
    void         HandleSubscriberDeleted  (QObject* Subscriber);

  private:
    QObject                               *m_parent;
    QString                                m_version;
    QMetaObject                            m_metaObject;
    QMap<QString,MethodParameters*>        m_methods;
    QMap<int,int>                          m_properties;
    QList<QObject*>                        m_subscribers;
    QMutex                                *m_subscriberLock;
};

Q_DECLARE_METATYPE(TorcHTTPService*);
#endif // TORCSERVICE_H
