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
    static  TorcBonjour*     Instance(void);
    static  void             TearDown(void);

    void*   Register        (quint16 Port, const QByteArray &Type,
                             const QByteArray &Name, const QByteArray &Txt);
    void*   Browse          (const QByteArray &Type);
    void    Deregister      (void* Reference);

  private slots:
    void    socketReadyRead (int Socket);
    void    hostLookup      (QHostInfo);

  protected:
    TorcBonjour();
   ~TorcBonjour();

    TorcBonjourPriv *m_priv;
};

#endif
