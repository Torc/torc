#ifndef TORCOBSERVABLE_H
#define TORCOBSERVABLE_H

// Torc
#include "torccoreexport.h"
#include "torcevent.h"

class QObject;
class QMutex;

class TORC_CORE_PUBLIC TorcObservable
{
  public:
    TorcObservable();
    virtual ~TorcObservable();

    void            AddObserver    (QObject* Observer);
    void            RemoveObserver (QObject* Observer);
    void            Notify         (const TorcEvent &Event);

  private:
    QMutex         *m_observerLock;
    QList<QObject*> m_observers;
};

#endif // TORCOBSERVABLE_H
