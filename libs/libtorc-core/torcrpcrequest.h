#ifndef TORCRPCREQUEST_H
#define TORCRPCREQUEST_H

// Qt
#include <QList>
#include <QVariant>
#include <QJsonObject>

// Torc
#include "torccoreexport.h"
#include "http/torcwebsocket.h"
#include "torcreferencecounted.h"

class TORC_CORE_PUBLIC TorcRPCRequest : public TorcReferenceCounter
{
  public:
    enum State
    {
        None          = (0 << 0),
        RequestSent   = (1 << 0),
        ReplyReceived = (1 << 1),
        Cancelled     = (1 << 2),
        TimedOut      = (1 << 3),
        Errored       = (1 << 4),
        Result        = (1 << 5)
    };

  public:
    TorcRPCRequest(const QString &Method, QObject *Parent);
    TorcRPCRequest(const QString &Method);
    TorcRPCRequest(TorcWebSocket::WSSubProtocol Protocol, const QByteArray &Data);

    bool                IsNotification         (void);
    void                NotifyParent           (void);
    void                SetParent              (QObject *Parent);
    QByteArray&         SerialiseRequest       (TorcWebSocket::WSSubProtocol Protocol);

    void                AddState               (int State);
    void                SetID                  (int ID);
    void                AddParameter           (const QString &Name, const QVariant &Value);
    void                SetReply               (const QVariant &Reply);

    int                 GetState               (void);
    int                 GetID                  (void);
    QString             GetMethod              (void);
    const QVariant&     GetReply               (void);
    const QList<QPair<QString,QVariant> >&
                        GetParameters          (void);
    QByteArray&         GetData                (void);

  private:
    TorcRPCRequest(const QJsonObject &Object);
    ~TorcRPCRequest();

    void                ParseJSONObject        (const QJsonObject &Object);


  private:
    bool                m_notification;
    int                 m_state;
    int                 m_id;
    QString             m_method;
    QObject            *m_parent;
    QMutex             *m_parentLock;
    bool                m_validParent;
    QList<QPair<QString,QVariant> > m_parameters;
    QByteArray          m_serialisedData;
    QVariant            m_reply;
};

Q_DECLARE_METATYPE(TorcRPCRequest*);

#endif // TORCRPCREQUEST_H
