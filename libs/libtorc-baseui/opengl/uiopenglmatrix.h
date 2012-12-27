#ifndef UIOPENGLMATRIX_H
#define UIOPENGLMATRIX_H

// Torc
#include "uiopengldefs.h"

class UIOpenGLMatrix
{
  public:
    UIOpenGLMatrix();

    void    setToIdentity (void);
    void    rotate        (qreal Degrees);
    void    scale         (GLfloat Horizontal, GLfloat Vertical);
    void    translate     (GLfloat X, GLfloat Y);
    void    reflect       (bool Vertical);
    UIOpenGLMatrix & operator*=(const UIOpenGLMatrix &Other);

  public:
    GLfloat m[4][4];
    GLfloat alpha;
    GLfloat color;

  private:
    void product       (int Row, const UIOpenGLMatrix &Other);
};

#endif // UIOPENGLMATRIX_H
