#ifndef EVENTPROXY_H
#define EVENTPROXY_H

// Qt
#include <QWindow>
#include <QObject>

class EventProxy : public QObject
{
    Q_OBJECT

  public:
    EventProxy(QWindow *Window);
   ~EventProxy();

    bool event(QEvent *Event);

  private:
    QWindow *m_window;
};


#endif // EVENTPROXY_H
