#ifndef TORCOMXTUNNEL_H
#define TORCOMXTUNNEL_H

// Qt
#include <QMutex>

// Torc
#include "torcomxcore.h"
#include "torcomxbuffer.h"
#include "torcomxcomponent.h"

// OpenMaxIL
#include "OMX_Core.h"
#include "OMX_Component.h"

class TorcOMXTunnel
{
  public:
    TorcOMXTunnel(TorcOMXCore *Core, TorcOMXComponent *Source, TorcOMXComponent *Destination);
   ~TorcOMXTunnel();

    OMX_ERRORTYPE     Flush      (void);
    OMX_ERRORTYPE     Create     (void);
    OMX_ERRORTYPE     Destroy    (void);

    bool              m_connected;
    TorcOMXCore      *m_core;
    QMutex           *m_lock;
    TorcOMXComponent *m_source;
    OMX_U32           m_sourcePort;
    TorcOMXComponent *m_destination;
    OMX_U32           m_destinationPort;
};

#endif // TORCOMXTUNNEL_H
