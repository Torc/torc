#ifndef UIOPENGLVIEW_H
#define UIOPENGLVIEW_H

// Qt
#include <QStack>
#include <QRect>

// Torc
#include "torcbaseuiexport.h"
#include "uiopengldefs.h"
#include "uiopenglmatrix.h"

class UIEffect;

#define MAX_STACK_DEPTH 10

class TORC_BASEUI_PUBLIC UIOpenGLView
{
    enum Projection
    {
        ViewPortOnly,
        Parallel,
        Perspective
    };

  public:
    UIOpenGLView();
    virtual ~UIOpenGLView();

    void  SetViewPort        (const QRect &Rect, Projection Type = Parallel);
    QRect GetViewPort        (void);

protected:
    bool  InitialiseView     (const QRect &Rect);
    void  SetProjection      (Projection Type);
    bool  PushTransformation (const UIEffect *Effect, const QRectF *Dest);
    void  PopTransformation  (void);
    void  PushClipRect       (const QRect &Rect);
    void  PopClipRect        (void);

  protected:
    QRect                    m_viewport;
    QStack<QRect>            m_clips;
    UIOpenGLMatrix          *m_currentProjection;
    UIOpenGLMatrix           m_parallel;
    UIOpenGLMatrix           m_perspective;
    int                      m_currentTransformIndex;
    UIOpenGLMatrix           m_transforms[MAX_STACK_DEPTH];
};

#endif // UIOPENGLVIEW_H
