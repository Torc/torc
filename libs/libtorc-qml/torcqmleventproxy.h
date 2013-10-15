#ifndef TORCQMLEVENTPROXY_H
#define TORCQMLEVENTPROXY_H

// Qt
#include <QWindow>
#include <QObject>

// Torc
#include "torcqmlexport.h"

class TORC_QML_PUBLIC TorcQMLEventProxy : public QObject
{
    Q_OBJECT

  public:
    TorcQMLEventProxy(QWindow *Window);
   ~TorcQMLEventProxy();

    bool          event(QEvent *Event);

  signals:
    void          SceneGraphReady          (void);

  public slots:
    void          SceneGraphInitialized    (void);

  private:
    QWindow      *m_window;
};

#endif // TORCQMLEVENTPROXY_H
