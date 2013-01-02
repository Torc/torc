#ifndef UIIMAGETRACKER_H
#define UIIMAGETRACKER_H

// Qt
#include <QRect>
#include <QHash>

// Torc
#include "torcbaseuiexport.h"

class QPainterPath;
class QMutex;
class QImage;
class UIImage;
class UIFont;
class UIShapePath;
class UITextRenderer;

class TORC_BASEUI_PUBLIC UIImageTracker
{
  public:
    UIImageTracker();
    virtual ~UIImageTracker();

    void         UpdateImages         (void);
    UIImage*     AllocateImage        (const QString &Name,
                                       const QSize   &Size,
                                       const QString &FileName = QString(""));
    void         ReleaseImage         (UIImage       *Image);
    UIImage*     GetSimpleTextImage   (const QString &Text,
                                       const QRectF  *Rect,
                                       UIFont        *Font,
                                       int            Flags,
                                       int            Blur);
    UIImage*     GetShapeImage        (UIShapePath  *Path,
                                       const QRectF  *Rect);
    void         LoadImageFromFile    (UIImage *Image);
    void         ImageCompleted       (UIImage *Image, QImage *Text);

  protected:
    void         ExpireImages         (bool ExpireAll = false);
    void         SetMaximumCacheSizes (quint8  Hardware, quint8 Software,
                                       quint64 ExpirableItems);

  protected:
    bool                    m_synchronous;
    quint64                 m_hardwareCacheSize;
    quint64                 m_softwareCacheSize;
    quint64                 m_maxHardwareCacheSize;
    quint64                 m_maxSoftwareCacheSize;
    QList<UIImage*>         m_allocatedImages;
    QList<QString>          m_allocatedFileImages;
    QMutex                 *m_allocatedImageLock; // Is this needed?
    QHash<QString,UIImage*> m_imageHash;
    QList<UIImage*>         m_expireList;
    qint64                  m_maxExpireListSize;

    QHash<UIImage*,QImage*> m_completedImages;
    QMutex                 *m_completedImagesLock;
};

#endif // UIIMAGETRACKER_H
