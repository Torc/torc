#ifndef TORCQMLMEDIAELEMENT_H
#define TORCQMLMEDIAELEMENT_H

// Qt
#include <QtQuick/QQuickItem>

// Torc
#include "torcqmlexport.h"

class TORC_QML_PUBLIC TorcQMLMediaElement : public QQuickItem
{
    Q_OBJECT

  public:
    explicit TorcQMLMediaElement(QQuickItem *Parent = NULL);
    virtual ~TorcQMLMediaElement();
    
  signals:
    
  public slots:
    
  public:
    QSGNode*     updatePaintNode          (QSGNode* Node, UpdatePaintNodeData*);
};

#endif // TORCQMLMEDIAELEMENT_H
