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

    quint32 Register        (quint16 Port, const QByteArray &Type,
                             const QByteArray &Name, const QByteArray &Txt);
    quint32 Browse          (const QByteArray &Type);
    void    Deregister      (quint32 Reference);

  protected slots:
    bool    event           (QEvent *Event);
    void    SuspendPriv     (bool Suspend);

  private slots:
    void    socketReadyRead (int Socket);
    void    hostLookup      (QHostInfo);


  protected:
    TorcBonjour();
   ~TorcBonjour();

  private:
    TorcBonjourPriv *m_priv;
};

#endif
