#ifndef UIEDID_H
#define UIEDID_H

// Qt
#include <QByteArray>

// Torc
#include "torccoreexport.h"

class QMutex;

class TORC_CORE_PUBLIC UIEDID
{
  public:
    UIEDID                           (const QByteArray &Data);
   ~UIEDID();

    static void      RegisterEDID    (QByteArray Data);
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

#endif // UIEDID_H
