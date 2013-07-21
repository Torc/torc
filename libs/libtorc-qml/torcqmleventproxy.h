#ifndef TORCQMLEVENTPROXY_H
#define TORCQMLEVENTPROXY_H

// Qt
#include <QWindow>
#include <QObject>

class TorcQMLEventProxy : public QObject
{
    Q_OBJECT

  public:
    TorcQMLEventProxy(QWindow *Window);
   ~TorcQMLEventProxy();

    bool event(QEvent *Event);

  private:
    QWindow *m_window;
};

#endif // TORCQMLEVENTPROXY_H
