#ifndef _MYTH_THREAD_H_
#define _MYTH_THREAD_H_

// Qt
#include <QThread>

// Torc
#include "torccoreexport.h"

class QStringList;
class TorcThreadInternal;

class TORC_CORE_PUBLIC TorcThread
{
    friend class TorcThreadInternal;
    friend class UITimer;

  public:

    explicit TorcThread(const QString &ObjectName);
    virtual ~TorcThread();

    // TorcThread
    static void  Initialise(void);
    QThread*     GetQThread(void);
    static QThread* GetQThread(const QString &Thread);
    static bool  IsMainThread(void);
    static bool  IsCurrentThread(TorcThread *Thread);
    static bool  IsCurrentThread(QThread *Thread);
    static void  Cleanup(void);
    static void  GetAllThreadNames(QStringList &List);
    static void  GetAllRunningThreadNames(QStringList &List);
    static void  ThreadSetup(const QString &Name);
    static void  ThreadCleanup(void);

    // QThread shadow functions
    void         exit(int retcode = 0);
    bool         isFinished(void) const;
    bool         isRunning(void) const;
    QThread::Priority priority(void) const;
    void         setPriority(QThread::Priority priority);
    void         setStackSize(uint stackSize);
    uint         stackSize(void) const;
    bool         wait(unsigned long time = ULONG_MAX);
    void         setObjectName(const QString &name);
    QString      objectName(void) const;

    // QThread shadow slots
    void         quit(void);
    void         start(QThread::Priority = QThread::InheritPriority);
    void         terminate(void);

  protected:
    // TorcThread
    void         RunProlog(void);
    void         RunEpilog(void);

    // QThread shadow functions
    virtual void run(void);
    int          exec(void);

    // QThread shadow members
    static void  msleep(unsigned long time);
    static void  setTerminationEnabled(bool enabled = true);
    static void  sleep(unsigned long time);
    static void  usleep(unsigned long time);

  protected:
    TorcThreadInternal *m_thread;
    bool         m_prologExecuted;
    bool         m_epilogExecuted;
};

#endif // _MYTH_THREAD_H_
