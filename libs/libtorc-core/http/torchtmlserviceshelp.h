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

  public:
    TorcHTMLServicesHelp(TorcHTTPServer *Server);
    void ProcessHTTPRequest(TorcHTTPServer *Server, TorcHTTPRequest *Request, TorcHTTPConnection* Connection);

  public slots:
    QVariantMap GetServiceList (void);

  private:
    TorcHTTPServer *m_server;
};

#endif // TORCHTMLSERVICESHELP_H
