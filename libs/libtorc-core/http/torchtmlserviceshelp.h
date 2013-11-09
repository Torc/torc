#ifndef TORCHTMLSERVICESHELP_H
#define TORCHTMLSERVICESHELP_H

// Qt
#include <QObject>
#include <QVariant>

// Torc
#include "torccoreexport.h"
#include "torchttpservice.h"

class TorcHTTPServer;
class TorcHTTPRequest;
class TorcHTTPConnection;

class TORC_CORE_PUBLIC TorcHTMLServicesHelp : public QObject, public TorcHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",        "1.0.0")
    Q_CLASSINFO("GetServiceList", "type=services")
    Q_CLASSINFO("GetStartTime",   "type=starttime")
    Q_CLASSINFO("GetPriority",    "type=priority")
    Q_CLASSINFO("GetUuid",        "type=uuid")
    Q_CLASSINFO("GetDetails",     "type=details")

    Q_PROPERTY(QMap serviceList READ GetServiceList NOTIFY ServiceListChanged)

  public:
    TorcHTMLServicesHelp(TorcHTTPServer *Server);
    virtual ~TorcHTMLServicesHelp();

    void ProcessHTTPRequest(TorcHTTPRequest *Request, TorcHTTPConnection* Connection);

  signals:
    void           ServiceListChanged   (void);

  public slots:
    void           SubscriberDeleted    (QObject *Subscriber);
    QVariantMap    GetDetails           (void);
    QVariantMap    GetServiceList       (void);
    qint64         GetStartTime         (void);
    int            GetPriority          (void);
    QString        GetUuid              (void);
    void           HandlersChanged      (void);

  private:
    QMap<QString,QString> serviceList;
};

#endif // TORCHTMLSERVICESHELP_H
