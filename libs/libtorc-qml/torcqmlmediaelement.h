#ifndef TORCQMLMEDIAELEMENT_H
#define TORCQMLMEDIAELEMENT_H

// Qt
#include <QTimer>
#include <QQuickItem>

// Torc
#include "torcplayer.h"
#include "torcqmlexport.h"

class VideoColourSpace;
class TorcSGVideoPlayer;
class TorcSGVideoProvider;

class TORC_QML_PUBLIC TorcQMLMediaElement : public QQuickItem, public TorcPlayerInterface
{
    Q_OBJECT
    Q_CLASSINFO("Version",    "1.0.0")

  public:
    explicit TorcQMLMediaElement(QQuickItem *Parent = NULL);
    virtual ~TorcQMLMediaElement();

    QSGNode*             updatePaintNode          (QSGNode* Node, UpdatePaintNodeData*);
    
  public slots:
    void                 SubscriberDeleted        (QObject *Subscriber);
    bool                 InitialisePlayer         (void);

  protected slots:
    void                 Cleanup                  (void);
    void                 PlayerStateChanged       (TorcPlayer::PlayerState State);
    void                 TextureChanged           (void);

    // QQuickItem
    void                 geometryChanged          (const QRectF &NewGeometry, const QRectF &OldGeometry);

  protected:
    bool                 event                    (QEvent *Event);
    void                 mousePressEvent          (QMouseEvent *Event);
    void                 mouseReleaseEvent        (QMouseEvent *Event);
    void                 mouseDoubleClickEvent    (QMouseEvent *Event);
    void                 mouseMoveEvent           (QMouseEvent *Event);
    void                 wheelEvent               (QWheelEvent *Event);
    void                 hoverMoveEvent           (QHoverEvent *Event);

  private:
    VideoColourSpace    *m_videoColourSpace;
    TorcSGVideoPlayer   *m_videoPlayer;
    TorcSGVideoProvider *m_videoProvider;
    QTimer              *m_refreshTimer;
    bool                 m_textureStale;
    bool                 m_geometryStale;

    QRectF               m_mediaRect;
};

#endif // TORCQMLMEDIAELEMENT_H
