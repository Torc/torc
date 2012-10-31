#ifndef TORCHTTPHANDLER_H
#define TORCHTTPHANDLER_H

// Qt
#include <QString>

// Torc
#include "torccoreexport.h"

class TorcHTTPServer;
class TorcHTTPConnection;
class TorcHTTPRequest;

class TORC_CORE_PUBLIC TorcHTTPHandler
{
  public:
    TorcHTTPHandler(const QString &Signature);
    virtual ~TorcHTTPHandler();

    QString        Signature      (void);
    virtual void   ProcessRequest (TorcHTTPServer* Server, TorcHTTPRequest *Request, TorcHTTPConnection *Connection) = 0;

  private:
    QString        m_signature;
};

#endif // TORCHTTPHANDLER_H
