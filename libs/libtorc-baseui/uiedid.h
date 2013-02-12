#ifndef UIEDID_H
#define UIEDID_H

// Qt
#include <QMap>
#include <QPair>
#include <QWidget>
#include <QByteArray>

// Torc
#include "torcbaseuiexport.h"

class QMutex;

class TORC_BASEUI_PUBLIC UIEDID
{
  public:
    UIEDID                           (const QByteArray &Data);
   ~UIEDID();

    static void      RegisterEDID    (WId Window, int Screen);
    static qint16    PhysicalAddress (void);
    static int       GetAudioLatency (bool Interlaced);
    static int       GetVideoLatency (bool Interlaced);

    QString          GetMSString     (void);

  protected:
    UIEDID();

  protected:
    static UIEDID*   gUIEDID;
    static QMutex*   gUIEDIDLock;
    QByteArray       m_edidData;

  private:
    void             Process         (bool Quiet = false);

  private:
    qint16           m_physicalAddress;
    int              m_audioLatency;
    int              m_videoLatency;
    int              m_interlacedAudioLatency;
    int              m_interlacedVideoLatency;
    QString          m_productString;
    QString          m_name;
    QString          m_serialNumberStr;
    quint32          m_serialNumber;
};

class TORC_BASEUI_PUBLIC EDIDFactory
{
  public:
    EDIDFactory();
    virtual            ~EDIDFactory    ();
    static EDIDFactory* GetEDIDFactory (void);
    EDIDFactory*        NextFactory    (void) const;
    virtual void        GetEDID        (QMap<QPair<int,QString>,QByteArray > &EDIDMap,
                                        WId Window, int Screen) = 0;

  protected:
    static EDIDFactory* gEDIDFactory;
    EDIDFactory*        nextEDIDFactory;
};

#endif // UIEDID_H
