#ifndef UIOPENGLSHADERS_H
#define UIOPENGLSHADERS_H

// Qt
#include <QHash>

// Torc
#include "uiopengldefs.h"

typedef enum DefaultShaders
{
    kShaderDefault = 0,
    kShaderStudio,
    kShaderCount
} DefaultShaders;

class GLShaderObject
{
  public:
    GLShaderObject(uint Vertex, uint Fragment);
    GLShaderObject();

    GLuint m_vertexShader;
    GLuint m_fragmentShader;
};

class UIOpenGLShaders
{
  public:
    UIOpenGLShaders();
    virtual ~UIOpenGLShaders();

    uint CreateShaderObject        (const QByteArray &Vertex, const QByteArray &Fragment);
    void DeleteShaderObject        (uint  Object);
    void EnableShaderObject        (uint  Object);
    void SetShaderParams           (uint  Object, GLfloat* Values, const char* Uniform);

  protected:
    bool InitialiseShaders         (const QString &Extensions, GLType Type, bool UseRects);
    void CreateDefaultShaders      (void);
    void DeleteDefaultShaders      (void);
    uint CreateShader              (int Type, const QByteArray &Source);
    bool ValidateShaderObject      (uint Object);
    bool CheckObjectStatus         (uint Object);
    void OptimiseShaderSource      (QByteArray &Source);
    void DeleteShaders             (void);

  protected:
    bool                           m_valid;
    bool                           m_usingRects;
    QByteArray                     m_qualifiers;
    QByteArray                     m_GLSLVersion;
    uint                           m_active_obj;
    QHash<GLuint, GLShaderObject>  m_shaderObjects;
    uint                           m_shaders[kShaderCount];
    GLfloat                        m_parameters[4][4];
    int                            m_maxTextureUnits;

    TORC_GLGETSHADERIVPROC         m_glGetShaderiv;
    TORC_GLCREATESHADERPROC        m_glCreateShader;
    TORC_GLSHADERSOURCEPROC        m_glShaderSource;
    TORC_GLCOMPILESHADERPROC       m_glCompileShader;
    TORC_GLGETSHADERINFOLOGPROC    m_glGetShaderInfoLog;
    TORC_GLCREATEPROGRAMPROC       m_glCreateProgram;
    TORC_GLATTACHSHADERPROC        m_glAttachShader;
    TORC_GLLINKPROGRAMPROC         m_glLinkProgram;
    TORC_GLUSEPROGRAMPROC          m_glUseProgram;
    TORC_GLGETPROGRAMINFOLOGPROC   m_glGetProgramInfoLog;
    TORC_GLGETPROGRAMIVPROC        m_glGetProgramiv;
    TORC_GLDETACHSHADERPROC        m_glDetachShader;
    TORC_GLDELETESHADERPROC        m_glDeleteShader;
    TORC_GLDELETEPROGRAMPROC       m_glDeleteProgram;
    TORC_GLGETUNIFORMLOCATIONPROC  m_glGetUniformLocation;
    TORC_GLUNIFORMMATRIX4FVPROC    m_glUniformMatrix4fv;
    TORC_GLBINDATTRIBLOCATIONPROC  m_glBindAttribLocation;
    TORC_GLVERTEXATTRIBPOINTERPROC m_glVertexAttribPointer;
    TORC_GLENABLEVERTEXATTRIBARRAYPROC  m_glEnableVertexAttribArray;
    TORC_GLDISABLEVERTEXATTRIBARRAYPROC m_glDisableVertexAttribArray;
    TORC_GLVERTEXATTRIB4FPROC      m_glVertexAttrib4f;

};

#endif // UIOPENGLSHADERS_H
