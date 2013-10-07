#ifndef TORCQMLFRAMERATE_H
#define TORCQMLFRAMERATE_H

// Qt
#include <QTimerEvent>
#include <QObject>

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
    void      framesPerSecondChanged  (void);

  public slots:
    qreal     GetFramesPerSecond      (void);
    void      NewFrame                (void);

  protected:
    void      timerEvent              (QTimerEvent *Event);

  private:
    int       m_timer;
    int       m_count;
    qreal     framesPerSecond;
};

#endif // TORCQMLFRAMERATE_H
