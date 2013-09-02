#ifndef TORCQTHREAD_H
#define TORCQTHREAD_H

// Qt
#include <QThread>

// Torc
#include "torccoreexport.h"

class TorcQThread : public QThread
{
    Q_OBJECT

  public:
    explicit TorcQThread(const QString &Name);
    virtual ~TorcQThread();
    
  signals:
    void            Started          (void);
    void            Finished         (void);

  public:
    virtual void    Start            (void) = 0;
    virtual void    Finish           (void) = 0;

  protected:
    virtual void    run              (void);

  private:
    void            Initialise       (void);
    void            Deinitialise     (void);
};

#endif // TORCQTHREAD_H
