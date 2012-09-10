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
    static bool      Ready           (void);
    static bool      Provisional     (void);
    static void      RegisterEDID    (QByteArray Data, bool Provisional);
    static qint16    PhysicalAddress (void);

  protected:
    TorcEDID();
   ~TorcEDID();

  protected:
    static TorcEDID* gTorcEDID;
    static QMutex*   gTorcEDIDLock;

    bool             m_edidDataProvisional;
    QByteArray       m_edidData;

  private:
    void             Process         (void);

  private:
    qint16           m_physicalAddress;
};

#endif // TORCEDID_H
