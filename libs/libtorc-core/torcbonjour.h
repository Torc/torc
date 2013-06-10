#ifndef TORCBONJOUR_H
#define TORCBONJOUR_H

// Qt
#include <QObject>
#include <QHostInfo>

// Torc
#include "torccoreexport.h"

class TorcBonjourPriv;

class TORC_CORE_PUBLIC TorcBonjour : public QObject
{
    Q_OBJECT

  public:
    static  TorcBonjour*     Instance (void);
    static  void             Suspend  (bool Suspend);
    static  void             TearDown (void);
    static  QByteArray       MapToTxtRecord (const QMap<QByteArray,QByteArray> &Map);
    static  QMap<QByteArray,QByteArray> TxtRecordToMap(const QByteArray &TxtRecord);

    quint32 Register        (quint16 Port, const QByteArray &Type,
                             const QByteArray &Name, const QByteArray &Txt);
    quint32 Browse          (const QByteArray &Type);
    void    Deregister      (quint32 Reference);

  protected:
    bool    event           (QEvent *Event);

  private slots:
    void    SuspendPriv     (bool Suspend);
    void    socketReadyRead (int Socket);
    void    HostLookup      (const QHostInfo &HostInfo);

  protected:
    TorcBonjour();
   ~TorcBonjour();

  private:
    TorcBonjourPriv *m_priv;
};

#endif
