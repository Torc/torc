#ifndef TORCRPCREQUEST_H
#define TORCRPCREQUEST_H

// Qt
#include <QList>
#include <QVariant>

// Torc
#include "torccoreexport.h"
#include "http/torcwebsocket.h"
#include "torcreferencecounted.h"

class TORC_CORE_PUBLIC TorcRPCRequest : public TorcReferenceCounter
{
  public:
    enum State
    {
        None           = (0 << 0),
        RequestSent    = (1 << 0),
        ReplieReceived = (1 << 1),
        Cancelled      = (1 << 2),
        TimedOut       = (1 << 3)
    };

  public:
    TorcRPCRequest(const QString &Method, QObject *Parent);
    TorcRPCRequest(const QString &Method);

    QByteArray&     Serialise              (TorcWebSocket::WSSubProtocol Protocol);

    void            AddState               (int State);
    void            SetID                  (int ID);
    void            AddParameter           (const QString &Name, const QVariant &Value);

    int             GetState               (void);
    int             GetID                  (void);
    QString         GetMethod              (void);
    QObject*        GetParent              (void);
    const QList<QPair<QString,QVariant> >&
                    GetParameters          (void);

  private:
    ~TorcRPCRequest();

    int             m_state;
    int             m_id;
    QString         m_method;
    QObject        *m_parent;
    QList<QPair<QString,QVariant> > m_parameters;
    QByteArray      m_serialisedData;
};

Q_DECLARE_METATYPE(TorcRPCRequest*);

#endif // TORCRPCREQUEST_H
