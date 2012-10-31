#ifndef TORCHTMLHANDLER_H
#define TORCHTMLHANDLER_H

// Torc
#include "torccoreexport.h"
#include "torchttphandler.h"

class TorcHTTPServer;
class TorcHTTPRequest;
class TorcHTTPConnection;

class TORC_CORE_PUBLIC TorcHTMLHandler : public TorcHTTPHandler
{
  public:
    TorcHTMLHandler(const QString &Path);
    virtual void ProcessHTTPRequest(TorcHTTPServer *Server, TorcHTTPRequest *Request, TorcHTTPConnection *Connection);
};

#endif // TORCHTMLHANDLER_H
