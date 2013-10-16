#ifndef TORCSGVIDEOPLAYER_H
#define TORCSGVIDEOPLAYER_H

// Qt
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
    bool                   HandleAction           (int Action);
    void                   SetVideoProvider       (TorcSGVideoProvider *Provider);
    TorcSGVideoProvider*   GetVideoProvider       (void);

  signals:
    void                   ResetRequest           (void);

  protected:
    virtual void           Teardown               (void);

  private slots:
    void                   HandleReset            (void);

  private:
    TorcSGVideoProvider   *m_videoProvider;
    VideoFrame            *m_currentFrame;
    int                    m_manualAVSyncAdjustment;
    WaitState              m_waitState;
    QElapsedTimer          m_waitTimer;
};

#endif // TORCSGVIDEOPLAYER_H
