#ifndef TORCREFERENCECOUNTED_H_
#define TORCREFERENCECOUNTED_H_

// Qt
#include <QAtomicInt>

// Torc
#include "torccoreexport.h"

class TORC_CORE_PUBLIC TorcReferenceCounter
{
  public:
    static void EventLoopEnding(bool Ending);

    TorcReferenceCounter(void);
    virtual ~TorcReferenceCounter(void);

    void UpRef(void);
    bool DownRef(void);
    bool IsShared(void);

  private:
    QAtomicInt   m_refCount;
    static bool  m_eventLoopEnding;
};

class TORC_CORE_PUBLIC TorcReferenceLocker
{
  public:
    TorcReferenceLocker(TorcReferenceCounter *Counter);
   ~TorcReferenceLocker();

  private:
    TorcReferenceCounter *m_refCountedObject;
};

#endif
