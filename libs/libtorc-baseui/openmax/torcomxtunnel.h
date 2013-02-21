#ifndef TORCOMXTUNNEL_H
#define TORCOMXTUNNEL_H

// Qt
#include <QMutex>

// Torc
#include "torcomxcore.h"
#include "torcomxport.h"
#include "torcomxcomponent.h"

// OpenMaxIL
#include "IL/OMX_Core.h"
#include "IL/OMX_Component.h"

class TorcOMXTunnel
{
  public:
    TorcOMXTunnel(TorcOMXCore *Core, TorcOMXComponent *Source, OMX_U32 SourceIndex,
                  TorcOMXComponent *Destination, OMX_U32 DestinationIndex);
   ~TorcOMXTunnel();

    bool              IsConnected (void);
    OMX_ERRORTYPE     Flush       (void);
    OMX_ERRORTYPE     Create      (void);
    OMX_ERRORTYPE     Destroy     (void);

  private:
    bool              m_connected;
    TorcOMXCore      *m_core;
    QMutex           *m_lock;
    TorcOMXComponent *m_source;
    OMX_U32           m_sourceIndex;
    TorcOMXComponent *m_destination;
    OMX_U32           m_destinationIndex;
};

#endif // TORCOMXTUNNEL_H
