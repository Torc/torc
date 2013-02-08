#ifndef TORCEDID_H
#define TORCEDID_H

// Qt
#include <QByteArray>

// Torc
#include "torccoreexport.h"

class QMutex;

class TORC_CORE_PUBLIC TorcEDID
{
  public:
    TorcEDID                         (const QByteArray &Data);
   ~TorcEDID();

    static void      RegisterEDID    (QByteArray Data);
    static qint16    PhysicalAddress (void);
    static int       GetAudioLatency (bool Interlaced);
    static int       GetVideoLatency (bool Interlaced);

    QString          GetMSString     (void);

  protected:
    TorcEDID();

  protected:
    static TorcEDID* gTorcEDID;
    static QMutex*   gTorcEDIDLock;
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
};

#endif // TORCEDID_H
