#ifndef TORCHTMLSERVICESHELP_H
#define TORCHTMLSERVICESHELP_H

// Torc
#include "torccoreexport.h"
#include "torchttphandler.h"

class TorcHTTPServer;
class TorcHTTPRequest;
class TorcHTTPConnection;

class TORC_CORE_PUBLIC TorcHTMLServicesHelp : public TorcHTTPHandler
{
  public:
    TorcHTMLServicesHelp(const QString &Path, const QString &Name);
    virtual void ProcessHTTPRequest(TorcHTTPServer *Server, TorcHTTPRequest *Request, TorcHTTPConnection*);
};

#endif // TORCHTMLSERVICESHELP_H
