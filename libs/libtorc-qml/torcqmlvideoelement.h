#ifndef TORCQMLVIDEOELEMENT_H
#define TORCQMLVIDEOELEMENT_H

// Qt
#include <QtQuick/QQuickItem>

class TorcQMLVideoElement : public QQuickItem
{
    Q_OBJECT

  public:
    explicit TorcQMLVideoElement(QQuickItem *Parent = NULL);
    virtual ~TorcQMLVideoElement();
    
  signals:
    
  public slots:
    
  public:
    QSGNode*     updatePaintNode          (QSGNode* Node, UpdatePaintNodeData*);
};

#endif // TORCQMLVIDEOELEMENT_H
