#ifndef TORCOMXBUFFER_H
#define TORCOMXBUFFER_H

// Qt
#include <QMutex>
#include <QQueue>
#include <QWaitCondition>

// Torc
#include "torcomxcore.h"
#include "torcomxcomponent.h"

// OpenMaxIL
#include "IL/OMX_Core.h"
#include "IL/OMX_Component.h"

class TorcOMXBuffer
{
  public:
    TorcOMXBuffer(TorcOMXComponent *Parent, OMX_HANDLETYPE Handle, OMX_U32 Port);
   ~TorcOMXBuffer();

    OMX_ERRORTYPE                 EnablePort    (bool Enable);
    OMX_ERRORTYPE                 Create        (bool Allocate);
    OMX_ERRORTYPE                 Destroy       (void);
    OMX_ERRORTYPE                 Flush         (void);
    OMX_ERRORTYPE                 MakeAvailable (OMX_BUFFERHEADERTYPE* Buffer);
    OMX_BUFFERHEADERTYPE*         GetBuffer     (OMX_S32 Timeout);

    TorcOMXComponent             *m_parent;
    OMX_HANDLETYPE                m_handle;
    OMX_U32                       m_port;
    QMutex                       *m_lock;
    QWaitCondition               *m_wait;
    bool                          m_createdBuffers;
    QList<OMX_BUFFERHEADERTYPE*>  m_buffers;
    QQueue<OMX_BUFFERHEADERTYPE*> m_availableBuffers;
    OMX_U32                       m_alignment;
};

#endif // TORCOMXBUFFER_H
