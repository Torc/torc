#ifndef TORCRAOPCONNECTION_H
#define TORCRAOPCONNECTION_H

// Qt
#include <QTextStream>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QObject>

// Torc
#include "torcreferencecounted.h"

#include <openssl/rsa.h>

class TorcRAOPConnectionPriv;

class TorcRAOPConnection : public QObject
{
    Q_OBJECT

    friend class TorcRAOPDevice;
    friend class TorcRAOPBuffer;

  protected slots:
    void           ReadText     (void);
    void           ReadData     (void);

  protected:
    static RSA*    LoadKey      (void);
    QTcpSocket*    MasterSocket (void);
    QByteArray*    Read         (void);

  protected:
    TorcRAOPConnection(QTcpSocket *Socket, int Reference, const QString &MACAddress);
    virtual ~TorcRAOPConnection();

    bool           Open         (void);
    void           Close        (void);
    void           ParseHeader  (const QByteArray &Line, bool First);
    void           ProcessText  (void);
    void           SendResend   (quint64 Timestamp, quint16 Start, quint16 End);
    void           timerEvent   (QTimerEvent *Event);

  private:
    int                     m_reference;
    TorcRAOPConnectionPriv *m_priv;
};

#endif // TORCRAOPCONNECTION_H
