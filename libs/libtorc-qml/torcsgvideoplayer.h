#ifndef TORCSGVIDEOPLAYER_H
#define TORCSGVIDEOPLAYER_H

// Qt
#include <QMouseEvent>
#include <QElapsedTimer>

// Torc
#include "torcqmlexport.h"
#include "videoplayer.h"

class VideoColourSpace;
class TorcSGVideoProvider;

class TORC_QML_PUBLIC TorcSGVideoPlayer : public VideoPlayer
{
    Q_OBJECT

    enum WaitState
    {
        WaitStateNone       = 0,
        WaitStateNoAudio    = 1,
        WaitStateNoVideo    = 2,
        WaitStateVideoAhead = 3
    };

  public:
    TorcSGVideoPlayer(QObject* Parent, int PlaybackFlags, int DecodeFlags);
    virtual ~TorcSGVideoPlayer();

    bool                   Refresh                (quint64 TimeNow, const QSizeF &Size, bool Visible);
    void                   Render                 (quint64 TimeNow);
    void                   Reset                  (void);
    void                   SetVideoProvider       (TorcSGVideoProvider *Provider);
    bool                   event                  (QEvent *Event);

  public slots:
    void                   SetParentGeometry      (const QRectF &NewGeometry);

  signals:
    void                   ResetRequest           (void);

  protected:
    virtual void           Teardown               (void);

  private slots:
    void                   HandleReset            (void);

  private:
    bool                   m_resetVideoProvider;
    TorcSGVideoProvider   *m_videoProvider;
    VideoFrame            *m_currentFrame;
    qint64                 m_currentVideoPts;
    double                 m_currentFrameRate;
    int                    m_manualAVSyncAdjustment;
    WaitState              m_waitState;
    QElapsedTimer          m_waitTimer;

    QRectF                 m_parentGeometry;
    QPointF                m_mousePress;
};

#endif // TORCSGVIDEOPLAYER_H
