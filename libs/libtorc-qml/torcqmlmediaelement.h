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

  public:
    explicit TorcQMLMediaElement(QQuickItem *Parent = NULL);
    virtual ~TorcQMLMediaElement();

    QSGNode*             updatePaintNode          (QSGNode* Node, UpdatePaintNodeData*);

  signals:
    
  public slots:
    bool                 InitialisePlayer         (void);

  protected slots:
    void                 Cleanup                  (void);
    void                 PlayerStateChanged       (TorcPlayer::PlayerState State);
    void                 TextureChanged           (void);

  protected:
    bool                 event                    (QEvent *Event);

  private:
    VideoColourSpace    *m_videoColourSpace;
    TorcSGVideoProvider *m_videoProvider;
    QTimer              *m_refreshTimer;
    bool                 m_textureStale;
};

#endif // TORCQMLMEDIAELEMENT_H
