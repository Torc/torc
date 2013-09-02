#ifndef TORCRUNLOOPOSX_H
#define TORCRUNLOOPOSX_H

// OS X
#include <CoreFoundation/CoreFoundation.h>

// Torc
#include "torccoreexport.h"
#include "torcqthread.h"

class CallbackObject : public QObject
{
    Q_OBJECT

  public:
    CallbackObject();

  public slots:
    void Run (void);
};

class TorcOSXCallbackThread : public TorcQThread
{
    friend class TorcRunLoopOSX;

    Q_OBJECT

  protected:
    TorcOSXCallbackThread();
    ~TorcOSXCallbackThread();

  protected:
    void Start      (void);
    void Finish     (void);

  private:
    CallbackObject *m_object;
};

/*! \brief A reference to the global administration CFRunLoop.
 *
 *  This is used by various OS X objects that require a CFRunLoop (other
 *  than the main loop) for callback purposes.
 */
extern TORC_CORE_PUBLIC CFRunLoopRef gAdminRunLoop;

#endif // TORCRUNLOOPOSX_H
