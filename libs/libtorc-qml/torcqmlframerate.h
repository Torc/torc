#ifndef TORCQMLFRAMERATE_H
#define TORCQMLFRAMERATE_H

// Qt
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>

// Torc
#include "torcqmlexport.h"

class TORC_QML_PUBLIC TorcQMLFrameRate : public QObject
{
    Q_OBJECT

    Q_PROPERTY(qreal framesPerSecond READ GetFramesPerSecond NOTIFY framesPerSecondChanged())

  public:
    TorcQMLFrameRate();
   ~TorcQMLFrameRate();

   signals:
    void           framesPerSecondChanged  (void);

  public slots:
    qreal          GetFramesPerSecond      (void);
    void           NewFrame                (void);
    void           Timeout                 (void);

  private:
    void           Update                  (void);

  private:
    int            m_count;
    qreal          framesPerSecond;
    QTimer        *m_timeout;
    QElapsedTimer  m_timer;
};

#endif // TORCQMLFRAMERATE_H
