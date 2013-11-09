#ifndef TORCQMLMEDIAELEMENT_H
#define TORCQMLMEDIAELEMENT_H

// Qt
#include <QTimer>
#include <QQuickItem>

// Torc
#include "torcplayer.h"
#include "torcqmlexport.h"

class VideoColourSpace;
class TorcSGVideoProvider;

class TORC_QML_PUBLIC TorcQMLMediaElement : public QQuickItem, public TorcPlayerInterface
{
    Q_OBJECT
    Q_CLASSINFO("Version",    "1.0.0")

  public:
    explicit TorcQMLMediaElement(QQuickItem *Parent = NULL);
    virtual ~TorcQMLMediaElement();

    QSGNode*             updatePaintNode          (QSGNode* Node, UpdatePaintNodeData*);

  signals:
    
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

  private:
    VideoColourSpace    *m_videoColourSpace;
    TorcSGVideoProvider *m_videoProvider;
    QTimer              *m_refreshTimer;
    bool                 m_textureStale;
    bool                 m_geometryStale;
};

#endif // TORCQMLMEDIAELEMENT_H
