#ifndef TORCQMLEVENTPROXY_H
#define TORCQMLEVENTPROXY_H

// Qt
#include <QTimer>
#include <QMutex>
#include <QWindow>
#include <QObject>

// Torc
#include "torcqmlexport.h"

#define QTRENDER_THREAD QString("QtRender")

typedef void (*RenderCallback) (void*, int);

class TORC_QML_PUBLIC TorcRenderCallback
{
  public:
    TorcRenderCallback(RenderCallback Function, void *Object, int Parameter)
      : m_function(Function),
        m_object(Object),
        m_parameter(Parameter)
    {
    }

    RenderCallback m_function;
    void          *m_object;
    int            m_parameter;
};

class TorcQMLDisplay;

class TORC_QML_PUBLIC TorcQMLEventProxy : public QObject
{
    Q_OBJECT

  public:
    TorcQMLEventProxy(QWindow *Window, bool Hidemouse = false);
   ~TorcQMLEventProxy();

    void                      RegisterCallback         (RenderCallback Function, void* Object, int Parameter);
    void                      ProcessCallbacks         (void);
    TorcQMLDisplay*           GetDisplay               (void);
    QWindow*                  GetWindow                (void);

    bool                      eventFilter              (QObject *Object, QEvent *Event);

  protected:
    bool                      event                    (QEvent *Event);

  signals:
    void                      SceneGraphReady          (void);

  public slots:
    void                      SceneGraphInitialized    (void);
    void                      HideMouse                (void);

  private:
    QWindow                  *m_window;
    QMutex                   *m_callbackLock;
    QList<TorcRenderCallback> m_callbacks;
    QTimer                   *m_mouseTimer;
    bool                      m_mouseHidden;
    TorcQMLDisplay           *m_display;
};

#endif // TORCQMLEVENTPROXY_H
