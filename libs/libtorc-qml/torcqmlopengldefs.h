#ifndef TORCQMLOPENGLDEFS_H
#define TORCQMLOPENGLDEFS_H

// Qt
#include <QOpenGLContext>

#define GL_GLEXT_PROTOTYPES

#ifdef USING_X11
#define GLX_GLXEXT_PROTOTYPES
#define XMD_H 1
#ifndef GL_ES_VERSION_2_0
#include <GL/gl.h>
#endif
#undef GLX_ARB_get_proc_address
#endif // USING_X11

#ifdef _WIN32
#include <GL/gl.h>
#include "glext.h"
#endif

#define TORC_UYVY  0x8A1F

#ifndef GL_YCBCR_MESA
#define GL_YCBCR_MESA                     0x8757
#endif
#ifndef GL_YCBCR_422_APPLE
#define GL_YCBCR_422_APPLE                0x85B9
#endif
#ifndef GL_UNSIGNED_SHORT_8_8_MESA
#define GL_UNSIGNED_SHORT_8_8_MESA        0x85BA
#endif

// OpenGL ES 2.0 workarounds
#ifndef GL_RGBA
#define GL_RGBA 0x1908
#endif
#ifndef GL_BGRA
#define GL_BGRA  GL_RGBA
#endif
#ifndef GL_RGBA8
#define GL_RGBA8 GL_RGBA
#endif
// end workarounds

#endif // TORCQMLOPENGLDEFS_H
