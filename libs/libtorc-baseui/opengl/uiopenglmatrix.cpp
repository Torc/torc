/* Class UIOpenGLMatrix
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Std
#include "math.h"

// Torc
#include "torccompat.h"
#include "uiopenglmatrix.h"

UIOpenGLMatrix::UIOpenGLMatrix()
{
    setToIdentity();
}

void UIOpenGLMatrix::setToIdentity(void)
{
    alpha = 1.0f;
    color = 1.0f;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            m[i][j] = (i == j) ? 1.0f : 0.0f;
}

void UIOpenGLMatrix::rotate(qreal degrees)
{
    GLfloat rotation = degrees * (M_PI / 180.0);
    UIOpenGLMatrix rotate;
    rotate.m[0][0] = rotate.m[1][1] = cos(rotation);
    rotate.m[0][1] = sin(rotation);
    rotate.m[1][0] = -rotate.m[0][1];
    this->operator *=(rotate);
}

void UIOpenGLMatrix::scale(GLfloat horizontal, GLfloat vertical)
{
    UIOpenGLMatrix scale;
    scale.m[0][0] = horizontal;
    scale.m[1][1] = vertical;
    this->operator *=(scale);
}

void UIOpenGLMatrix::translate(GLfloat x, GLfloat y)
{
    UIOpenGLMatrix translate;
    translate.m[3][0] = x;
    translate.m[3][1] = y;
    this->operator *=(translate);
}

void UIOpenGLMatrix::reflect(bool Vertical)
{
    UIOpenGLMatrix reflect;
    reflect.m[0][0] = Vertical ? -1 : 1;
    reflect.m[1][1] = Vertical ? 1 : -1;
    this->operator *=(reflect);
}

UIOpenGLMatrix & UIOpenGLMatrix::operator*=(const UIOpenGLMatrix &r)
{
    for (int i = 0; i < 4; i++)
        product(i, r);
    return *this;
}

void UIOpenGLMatrix::product(int row, const UIOpenGLMatrix &r)
{
    GLfloat t0, t1, t2, t3;
    t0 = m[row][0] * r.m[0][0] + m[row][1] * r.m[1][0] + m[row][2] * r.m[2][0] + m[row][3] * r.m[3][0];
    t1 = m[row][0] * r.m[0][1] + m[row][1] * r.m[1][1] + m[row][2] * r.m[2][1] + m[row][3] * r.m[3][1];
    t2 = m[row][0] * r.m[0][2] + m[row][1] * r.m[1][2] + m[row][2] * r.m[2][2] + m[row][3] * r.m[3][2];
    t3 = m[row][0] * r.m[0][3] + m[row][1] * r.m[1][3] + m[row][2] * r.m[2][3] + m[row][3] * r.m[3][3];
    m[row][0] = t0; m[row][1] = t1; m[row][2] = t2; m[row][3] = t3;
}
