#ifndef UITIMER_H
#define UITIMER_H

class UITimer
{
  public:
    UITimer(qreal Interval);

    void Start       (void);
    void Wait        (void);
    void Reset       (void);
    void SetInterval (qreal Interval);


  protected:
    qreal   m_interval;
    quint64 m_nextEvent;
};

#endif // UITIMER_H
