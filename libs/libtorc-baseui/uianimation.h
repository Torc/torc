#ifndef UIANIMATION_H
#define UIANIMATION_H

// Qt
#include <QParallelAnimationGroup>

// Torc
#include "torcbaseuiexport.h"
#include "uiwidget.h"

class QDomElement;
class QParallelAnimationGroup;

class TORC_BASEUI_PUBLIC UIAnimation : public UIWidget
{
    Q_OBJECT

  public:
    static    int              kUIAnimationType;
    UIAnimation(UIWidget *Root, UIWidget *Parent, const QString &Name, int Flags);
    virtual ~UIAnimation();
    virtual   void             CopyFrom (UIWidget *Other);
    virtual   bool             Finalise (void);

    static    int              GetEasingCurve (const QString &EasingCurve);

  public slots:
    void      Start            (void);
    void      Stop             (void);

  signals:
    void      Started          (void);
    void      Finished         (void);

  protected:
    bool      InitialisePriv   (QDomElement *Element);
    void      CreateCopy       (UIWidget *Parent);
    void      CopyAnimation    (QAbstractAnimation *Parent, QAbstractAnimation *Other);


  private:
    bool      ParseAnimation   (QDomElement *Element, QAbstractAnimation *Parent);

  private:
    QParallelAnimationGroup   *m_animation;

    Q_DISABLE_COPY(UIAnimation);
};

Q_DECLARE_METATYPE(UIAnimation*);

#endif // UIANIMATION_H
