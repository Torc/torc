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
    static void      RegisterEDID    (QByteArray Data);
    static qint16    PhysicalAddress (void);
    static int       GetAudioLatency (bool Interlaced);
    static int       GetVideoLatency (bool Interlaced);

  protected:
    TorcEDID();
   ~TorcEDID();

  protected:
    static TorcEDID* gTorcEDID;
    static QMutex*   gTorcEDIDLock;
    QByteArray       m_edidData;

  private:
    void             Process         (void);

  private:
    qint16           m_physicalAddress;
    int              m_audioLatency;
    int              m_videoLatency;
    int              m_interlacedAudioLatency;
    int              m_interlacedVideoLatency;
};

#endif // TORCEDID_H
