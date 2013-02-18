#ifndef TORCOMXCOMPONENT_H
#define TORCOMXCOMPONENT_H

// Qt
#include <QMutex>
#include <QWaitCondition>

// Torc
#include "torcomxcore.h"

// OpenMaxIL
#include "OMX_Core.h"
#include "OMX_Component.h"

class TorcOMXBuffer;

class TorcOMXEvent
{
  public:
    TorcOMXEvent(OMX_EVENTTYPE Type, OMX_U32 Data1, OMX_U32 Data2);

    OMX_EVENTTYPE m_type;
    OMX_U32       m_data1;
    OMX_U32       m_data2;
};

class TorcOMXComponent
{
  public:
    static OMX_ERRORTYPE    EventHandlerCallback    (OMX_HANDLETYPE Component, OMX_PTR OMXComponent, OMX_EVENTTYPE Event, OMX_U32 Data1, OMX_U32 Data2, OMX_PTR EventData);
    static OMX_ERRORTYPE    EmptyBufferDoneCallback (OMX_HANDLETYPE Component, OMX_PTR OMXComponent, OMX_BUFFERHEADERTYPE *Buffer);
    static OMX_ERRORTYPE    FillBufferDoneCallback  (OMX_HANDLETYPE Component, OMX_PTR OMXComponent, OMX_BUFFERHEADERTYPE *Buffer);

  public:
    TorcOMXComponent(TorcOMXCore *Core, OMX_STRING Component, OMX_INDEXTYPE Index);
   ~TorcOMXComponent();

    bool                    IsValid                 (void);
    QString                 GetName                 (void);
    OMX_HANDLETYPE          GetHandle               (void);
    OMX_ERRORTYPE           SetState                (OMX_STATETYPE State);
    OMX_STATETYPE           GetState                (void);
    OMX_ERRORTYPE           SetParameter            (OMX_INDEXTYPE Index, OMX_PTR Structure);
    OMX_ERRORTYPE           GetParameter            (OMX_INDEXTYPE Index, OMX_PTR Structure);
    OMX_ERRORTYPE           SetConfig               (OMX_INDEXTYPE Index, OMX_PTR Structure);
    OMX_ERRORTYPE           GetConfig               (OMX_INDEXTYPE Index, OMX_PTR Structure);
    OMX_U32                 GetInputPort            (void);
    OMX_U32                 GetOutputPort           (void);
    OMX_ERRORTYPE           EnablePort              (bool Enable, bool Input);
    bool                    DisablePorts            (OMX_INDEXTYPE Index);
    OMX_ERRORTYPE           EmptyThisBuffer         (OMX_BUFFERHEADERTYPE *Buffer);
    OMX_ERRORTYPE           FillThisBuffer          (OMX_BUFFERHEADERTYPE *Buffer);
    OMX_ERRORTYPE           SetupInputBuffers       (bool Create);
    OMX_ERRORTYPE           SetupOutputBuffers      (bool Create);
    OMX_ERRORTYPE           DestroyInputBuffers     (void);
    OMX_ERRORTYPE           DestroyOutputBuffers    (void);
    OMX_BUFFERHEADERTYPE*   GetInputBuffer          (OMX_U32 Timeout);
    TorcOMXBuffer*          GetInputBuffers         (void);
    TorcOMXBuffer*          GetOutputBuffers        (void);
    OMX_ERRORTYPE           FlushBuffers            (bool Input, bool Output);
    OMX_ERRORTYPE           EventHandler            (OMX_HANDLETYPE Component, OMX_EVENTTYPE Event, OMX_U32 Data1, OMX_U32 Data2, OMX_PTR EventData);
    OMX_ERRORTYPE           EmptyBufferDone         (OMX_HANDLETYPE Component, OMX_BUFFERHEADERTYPE *Buffer);
    OMX_ERRORTYPE           FillBufferDone          (OMX_HANDLETYPE Component, OMX_BUFFERHEADERTYPE *Buffer);
    OMX_ERRORTYPE           SendCommand             (OMX_COMMANDTYPE Command, OMX_U32 Parameter, OMX_PTR Data);
    OMX_ERRORTYPE           WaitForResponse         (OMX_U32 Command, OMX_U32 Data2, OMX_U32 Timeout);

  protected:
    bool                    m_valid;
    TorcOMXCore            *m_core;
    OMX_HANDLETYPE          m_handle;
    QMutex                 *m_lock;
    QString                 m_componentName;
    OMX_INDEXTYPE           m_indexType;
    TorcOMXBuffer          *m_inputBuffer;
    TorcOMXBuffer          *m_outputBuffer;
    QList<TorcOMXEvent>     m_eventQueue;
    QMutex                 *m_eventQueueLock;
    QWaitCondition          m_eventQueueWait;
};

#endif

