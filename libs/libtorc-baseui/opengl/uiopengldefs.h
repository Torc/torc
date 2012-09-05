#ifndef UIOPENGLDEFS_H
#define UIOPENGLDEFS_H

// Qt
#include <QGLWidget>

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

typedef enum
{
    kGLUnknown = 0,
    kGLOpenGL2,
    kGLOpenGL2ES,
} GLType;

typedef enum
{
    kGLFeatNone    = 0x0000,
    kGLMultiTex    = 0x0001,
    kGLExtRect     = 0x0002,
    kGLExtFBufObj  = 0x0004,
    kGLExtPBufObj  = 0x0008,
    kGLNVFence     = 0x0010,
    kGLAppleFence  = 0x0020,
    kGLMesaYCbCr   = 0x0040,
    kGLAppleYCbCr  = 0x0080,
    kGLMipMaps     = 0x0100,
    kGLSL          = 0x0200,
    kGLVertexArray = 0x0400,
    kGLExtVBO      = 0x0800,
    kGLMaxFeat     = 0x1000,
} GLFeatures;

#define VERTEX_INDEX  0
#define COLOR_INDEX   1
#define TEXTURE_INDEX 2
#define VERTEX_SIZE   3
#define TEXTURE_SIZE  2

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

#ifndef GL_GENERATE_MIPMAP_SGIS
#define GL_GENERATE_MIPMAP_SGIS 0x8191
#endif

#ifndef GL_GENERATE_MIPMAP_HINT_SGIS
#define GL_GENERATE_MIPMAP_HINT_SGIS 0x8192
#endif

#ifndef GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS 0x8B4D
#endif

#ifndef GL_MAX_TEXTURE_UNITS
#define GL_MAX_TEXTURE_UNITS 0x84E2
#endif

#ifndef GL_TEXTTURE0
#define GL_TEXTURE0 0x84C0
#endif

#ifndef GL_TEXTURE_RECTANGLE_ARB
#define GL_TEXTURE_RECTANGLE_ARB 0x84F5
#endif

#ifndef GL_TEXTURE_RECTANGLE_EXT
#define GL_TEXTURE_RECTANGLE_EXT 0x84F5
#endif

#ifndef GL_TEXTURE_RECTANGLE_NV
#define GL_TEXTURE_RECTANGLE_NV 0x84F5
#endif

#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER          0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0    0x8CE0
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT 0x8CD6
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT 0x8CD7
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_DUPLICATE_ATTACHMENT
#define GL_FRAMEBUFFER_INCOMPLETE_DUPLICATE_ATTACHMENT 0x8CD8
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS
#define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS 0x8CD9
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_FORMATS
#define GL_FRAMEBUFFER_INCOMPLETE_FORMATS 0x8CDA
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER
#define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER 0x8CDB
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER
#define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER 0x8CDC
#endif
#ifndef GL_FRAMEBUFFER_UNSUPPORTED
#define GL_FRAMEBUFFER_UNSUPPORTED 0x8CDD
#endif

#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER               0x8892
#endif

#ifndef GL_PIXEL_UNPACK_BUFFER
#define GL_PIXEL_UNPACK_BUFFER        0x88EC
#endif

#ifndef GL_STREAM_DRAW
#define GL_STREAM_DRAW                0x88E0
#endif

#ifndef GL_WRITE_ONLY
#define GL_WRITE_ONLY                 0x88B9
#endif

#ifndef GL_NV_fence
#define GL_ALL_COMPLETED_NV               0x84F2
#endif

#ifndef GL_YCBCR_MESA
#define GL_YCBCR_MESA                     0x8757
#endif
#ifndef GL_YCBCR_422_APPLE
#define GL_YCBCR_422_APPLE                0x85B9
#endif
#ifndef GL_UNSIGNED_SHORT_8_8_MESA
#define GL_UNSIGNED_SHORT_8_8_MESA        0x85BA
#endif

#ifndef APIENTRY
#define APIENTRY
#endif

#ifndef GL_VERSION_2_0
typedef char GLchar;
#endif
#ifndef GL_ARB_shader_objects
typedef char GLcharARB;
#endif

#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER                0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER                  0x8B31
#endif
#ifndef GL_OBJECT_LINK_STATUS
#define GL_OBJECT_LINK_STATUS             0x8B82
#endif
#ifndef GL_OBJECT_INFO_LOG_LENGTH
#define GL_OBJECT_INFO_LOG_LENGTH         0x8B84
#endif

#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS                 0x8B81
#endif
#ifndef GL_INFO_LOG_LENGTH
#define GL_INFO_LOG_LENGTH                0x8B84
#endif

typedef GLuint ( * TORC_GLCREATESHADERPROC)
    (GLenum shaderType);
typedef void ( * TORC_GLSHADERSOURCEPROC)
    (GLuint shader, int numOfStrings, const char **strings, const int *lenOfStrings);
typedef void ( * TORC_GLCOMPILESHADERPROC)
    (GLuint shader);
typedef void ( * TORC_GLGETSHADERIVPROC)
    (GLuint shader, GLenum pname, GLint *params);
typedef void ( * TORC_GLGETSHADERINFOLOGPROC)
    (GLuint shader, GLint maxlength, GLint *length, GLchar *infolog);
typedef void ( * TORC_GLDELETEPROGRAMPROC)
    (GLuint shader);
typedef GLuint ( * TORC_GLCREATEPROGRAMPROC)
    (void);
typedef void ( * TORC_GLATTACHSHADERPROC)
    (GLuint program, GLuint shader);
typedef void ( * TORC_GLLINKPROGRAMPROC)
    (GLuint program);
typedef void ( * TORC_GLUSEPROGRAMPROC)
    (GLuint program);
typedef void ( * TORC_GLGETPROGRAMINFOLOGPROC)
    (GLuint object, int maxLen, int *len, char *log);
typedef void ( * TORC_GLGETPROGRAMIVPROC)
    (GLuint object, GLenum type, int *param);
typedef void ( * TORC_GLDETACHSHADERPROC)
    (GLuint program, GLuint shader);
typedef void ( * TORC_GLDELETESHADERPROC)
    (GLuint id);
typedef GLint ( * TORC_GLGETUNIFORMLOCATIONPROC)
    (GLuint program, const char *name);
typedef void  ( * TORC_GLUNIFORMMATRIX4FVPROC)
    (GLint location, GLint size, GLboolean transpose, const GLfloat *values);
typedef void ( * TORC_GLVERTEXATTRIBPOINTERPROC)
    (GLuint index, GLint size, GLenum type, GLboolean normalize,
     GLsizei stride, const GLvoid *ptr);
typedef void ( * TORC_GLENABLEVERTEXATTRIBARRAYPROC)
    (GLuint index);
typedef void ( * TORC_GLDISABLEVERTEXATTRIBARRAYPROC)
    (GLuint index);
typedef void ( * TORC_GLBINDATTRIBLOCATIONPROC)
    (GLuint program, GLuint index, const GLcharARB *name);
typedef void ( * TORC_GLVERTEXATTRIB4FPROC)
    (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);

typedef void (APIENTRY * TORC_GLACTIVETEXTUREPROC)
    (GLenum texture);

typedef ptrdiff_t TORC_GLsizeiptr;
typedef GLvoid* (APIENTRY * TORC_GLMAPBUFFERPROC)
    (GLenum target, GLenum access);
typedef void (APIENTRY * TORC_GLBINDBUFFERPROC)
    (GLenum target, GLuint buffer);
typedef void (APIENTRY * TORC_GLGENBUFFERSPROC)
    (GLsizei n, GLuint *buffers);
typedef void (APIENTRY * TORC_GLBUFFERDATAPROC)
    (GLenum target, TORC_GLsizeiptr size, const GLvoid *data, GLenum usage);
typedef GLboolean (APIENTRY * TORC_GLUNMAPBUFFERPROC)
    (GLenum target);
typedef void (APIENTRY * TORC_GLDELETEBUFFERSPROC)
    (GLsizei n, const GLuint *buffers);
typedef void (APIENTRY * TORC_GLGENFRAMEBUFFERSPROC)
    (GLsizei n, GLuint *framebuffers);
typedef void (APIENTRY * TORC_GLBINDFRAMEBUFFERPROC)
    (GLenum target, GLuint framebuffer);
typedef void (APIENTRY * TORC_GLFRAMEBUFFERTEXTURE2DPROC)
    (GLenum target, GLenum attachment,
     GLenum textarget, GLuint texture, GLint level);
typedef GLenum (APIENTRY * TORC_GLCHECKFRAMEBUFFERSTATUSPROC)
    (GLenum target);
typedef void (APIENTRY * TORC_GLDELETEFRAMEBUFFERSPROC)
    (GLsizei n, const GLuint *framebuffers);
typedef void (APIENTRY * TORC_GLDELETEFENCESNVPROC)
    (GLsizei n, const GLuint *fences);
typedef void (APIENTRY * TORC_GLGENFENCESNVPROC)
    (GLsizei n, GLuint *fences);
typedef void (APIENTRY * TORC_GLFINISHFENCENVPROC)
    (GLuint fence);
typedef void (APIENTRY * TORC_GLSETFENCENVPROC)
    (GLuint fence, GLenum condition);
typedef void ( * TORC_GLGENFENCESAPPLEPROC)
    (GLsizei n, GLuint *fences);
typedef void ( * TORC_GLDELETEFENCESAPPLEPROC)
    (GLsizei n, const GLuint *fences);
typedef void ( * TORC_GLSETFENCEAPPLEPROC)
    (GLuint fence);
typedef void ( * TORC_GLFINISHFENCEAPPLEPROC)
    (GLuint fence);

#endif // UIOPENGLDEFS_H
