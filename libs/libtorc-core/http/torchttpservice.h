#ifndef TORCSERVICE_H
#define TORCSERVICE_H

// Qt
#include <QMap>
#include <QMetaObject>

// Torc
#include "torccoreexport.h"
#include "torchttphandler.h"

class TorcHTTPServer;
class TorcHTTPRequest;
class TorcHTTPConnection;
class MethodParameters;

#define SERVICES_DIRECTORY QString("/services")

class TORC_CORE_PUBLIC TorcHTTPService : public TorcHTTPHandler
{
  public:
    TorcHTTPService(QObject *Parent, const QString &Signature, const QString &Name,
                    const QMetaObject &MetaObject, const QString &Blacklist = QString(""));
    virtual ~TorcHTTPService();

    void     ProcessHTTPRequest    (TorcHTTPServer *Server, TorcHTTPRequest *Request, TorcHTTPConnection *Connection);

  protected:
    void     UserHelp              (TorcHTTPServer *Server, TorcHTTPRequest *Request, TorcHTTPConnection *Connection);

  private:
    QObject                        *m_parent;
    QMetaObject                     m_metaObject;
    QMap<QString,MethodParameters*> m_methods;
};

#endif // TORCSERVICE_H
