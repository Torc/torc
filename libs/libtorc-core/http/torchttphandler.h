#ifndef TORCHTTPHANDLER_H
#define TORCHTTPHANDLER_H

// Qt
#include <QString>
#include <QVariantMap>

// Torc
#include "torccoreexport.h"

class TorcHTTPServer;
class TorcHTTPConnection;
class TorcHTTPRequest;

class TORC_CORE_PUBLIC TorcHTTPHandler
{
  public:
    TorcHTTPHandler(const QString &Signature, const QString &Name);
    virtual ~TorcHTTPHandler();

    QString             Signature          (void);
    bool                GetRecursive       (void);
    QString             Name               (void);
    virtual void        ProcessHTTPRequest (TorcHTTPRequest *Request, TorcHTTPConnection *Connection) = 0;
    virtual QVariantMap ProcessRequest     (const QString &Method, const QVariant &Parameters);

  protected:
    QString             m_signature;
    bool                m_recursive;
    QString             m_name;
};

#endif // TORCHTTPHANDLER_H
