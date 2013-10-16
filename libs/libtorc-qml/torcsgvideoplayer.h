#ifndef TORCSGVIDEOPLAYER_H
#define TORCSGVIDEOPLAYER_H

// Torc
#include "torcqmlexport.h"
#include "videoplayer.h"

class VideoColourSpace;
class TorcSGVideoProvider;

class TORC_QML_PUBLIC TorcSGVideoPlayer : public VideoPlayer
{
    Q_OBJECT

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
};

#endif // TORCSGVIDEOPLAYER_H
