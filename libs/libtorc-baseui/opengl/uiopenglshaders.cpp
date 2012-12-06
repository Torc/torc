/* Class UIOpenGLShaders
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

// Torc
#include "torclogging.h"
#include "uiopenglwindow.h"
#include "uiopenglshaders.h"

static const char kDefaultVertexShader[] =
"GLSL_DEFINES"
"attribute vec3 a_position;\n"
"attribute vec4 a_color;\n"
"attribute vec2 a_texcoord0;\n"
"varying   vec4 v_color;\n"
"varying   vec2 v_texcoord0;\n"
"uniform   mat4 u_projection;\n"
"uniform   mat4 u_transform;\n"
"void main() {\n"
"    gl_Position = u_projection * u_transform * vec4(a_position, 1.0);\n"
"    v_texcoord0 = a_texcoord0;\n"
"    v_color     = a_color;\n"
"}\n";

static const char kDefaultFragmentShader[] =
"GLSL_DEFINES"
"uniform GLSL_SAMPLER s_texture0;\n"
"varying vec4 v_color;\n"
"varying vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    gl_FragColor = GLSL_TEXTURE(s_texture0, v_texcoord0) * v_color;\n"
"}\n";

static const char kDefaultStudioFragmentShader[] =
"GLSL_DEFINES"
"uniform GLSL_SAMPLER s_texture0;\n"
"varying vec4 v_color;\n"
"varying vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    vec4 range   = vec4( 219.0 / 255.0, 219.0 / 255.0, 219.0 / 255.0, 1.0);\n"
"    vec4 offset  = vec4( 16.0  / 255.0, 16.0  / 255.0, 16.0  / 255.0, 0.0);\n"
"    gl_FragColor = (GLSL_TEXTURE(s_texture0, v_texcoord0) * v_color * range) + offset;\n"
"}\n";

GLShaderObject::GLShaderObject(uint Vertex, uint Fragment)
  : m_vertexShader(Vertex), m_fragmentShader(Fragment)
{
}

GLShaderObject::GLShaderObject()
  : m_vertexShader(0), m_fragmentShader(0)
{
}

UIOpenGLShaders::UIOpenGLShaders()
  : m_valid(false),
    m_usingRects(false),
    m_active_obj(0),
    m_glGetShaderiv(NULL),
    m_glCreateShader(NULL),
    m_glShaderSource(NULL),
    m_glCompileShader(NULL),
    m_glGetShaderInfoLog(NULL),
    m_glCreateProgram(NULL),
    m_glAttachShader(NULL),
    m_glLinkProgram(NULL),
    m_glUseProgram(NULL),
    m_glGetProgramInfoLog(NULL),
    m_glGetProgramiv(NULL),
    m_glDetachShader(NULL),
    m_glDeleteShader(NULL),
    m_glDeleteProgram(NULL),
    m_glGetUniformLocation(NULL),
    m_glUniformMatrix4fv(NULL),
    m_glBindAttribLocation(NULL),
    m_glVertexAttribPointer(NULL),
    m_glEnableVertexAttribArray(NULL),
    m_glDisableVertexAttribArray(NULL),
    m_glVertexAttrib4f(NULL)
{
    memset(m_shaders, 0, sizeof(uint) * kShaderCount);
    memset(m_parameters, 0, sizeof(GLfloat) * 4 * 4);
}

UIOpenGLShaders::~UIOpenGLShaders()
{
    DeleteDefaultShaders();
    DeleteShaders();
}

bool UIOpenGLShaders::InitialiseShaders(const QString &Extensions, GLType Type, bool UseRects)
{
    // max glsl texture units
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &m_maxTextureUnits);
    m_usingRects = UseRects;

    // define version and precision
    if (kGLOpenGL2 == Type)
    {
        m_GLSLVersion = "#version 110\n";
        m_qualifiers  = QByteArray();
    }
    else if (kGLOpenGL2ES == Type)
    {
        m_GLSLVersion = "#version 100\n";
        m_qualifiers  = "precision mediump float;\n";
    }

    m_glCreateShader = (TORC_GLCREATESHADERPROC)
        UIOpenGLWindow::GetProcAddress("glCreateShader");
    m_glShaderSource = (TORC_GLSHADERSOURCEPROC)
        UIOpenGLWindow::GetProcAddress("glShaderSource");
    m_glCompileShader = (TORC_GLCOMPILESHADERPROC)
        UIOpenGLWindow::GetProcAddress("glCompileShader");
    m_glGetShaderiv = (TORC_GLGETSHADERIVPROC)
        UIOpenGLWindow::GetProcAddress("glGetShaderiv");
    m_glGetShaderInfoLog = (TORC_GLGETSHADERINFOLOGPROC)
        UIOpenGLWindow::GetProcAddress("glGetShaderInfoLog");
    m_glDeleteProgram = (TORC_GLDELETEPROGRAMPROC)
        UIOpenGLWindow::GetProcAddress("glDeleteProgram");
    m_glCreateProgram = (TORC_GLCREATEPROGRAMPROC)
        UIOpenGLWindow::GetProcAddress("glCreateProgram");
    m_glAttachShader = (TORC_GLATTACHSHADERPROC)
        UIOpenGLWindow::GetProcAddress("glAttachShader");
    m_glLinkProgram = (TORC_GLLINKPROGRAMPROC)
        UIOpenGLWindow::GetProcAddress("glLinkProgram");
    m_glUseProgram = (TORC_GLUSEPROGRAMPROC)
        UIOpenGLWindow::GetProcAddress("glUseProgram");
    m_glGetProgramInfoLog = (TORC_GLGETPROGRAMINFOLOGPROC)
        UIOpenGLWindow::GetProcAddress("glGetProgramInfoLog");
    m_glGetProgramiv = (TORC_GLGETPROGRAMIVPROC)
        UIOpenGLWindow::GetProcAddress("glGetProgramiv");
    m_glDetachShader = (TORC_GLDETACHSHADERPROC)
        UIOpenGLWindow::GetProcAddress("glDetachShader");
    m_glDeleteShader = (TORC_GLDELETESHADERPROC)
        UIOpenGLWindow::GetProcAddress("glDeleteShader");
    m_glGetUniformLocation = (TORC_GLGETUNIFORMLOCATIONPROC)
        UIOpenGLWindow::GetProcAddress("glGetUniformLocation");
    m_glUniformMatrix4fv = (TORC_GLUNIFORMMATRIX4FVPROC)
        UIOpenGLWindow::GetProcAddress("glUniformMatrix4fv");
    m_glBindAttribLocation = (TORC_GLBINDATTRIBLOCATIONPROC)
        UIOpenGLWindow::GetProcAddress("glBindAttribLocation");
    m_glVertexAttribPointer = (TORC_GLVERTEXATTRIBPOINTERPROC)
        UIOpenGLWindow::GetProcAddress("glVertexAttribPointer");
    m_glEnableVertexAttribArray = (TORC_GLENABLEVERTEXATTRIBARRAYPROC)
        UIOpenGLWindow::GetProcAddress("glEnableVertexAttribArray");
    m_glDisableVertexAttribArray = (TORC_GLDISABLEVERTEXATTRIBARRAYPROC)
        UIOpenGLWindow::GetProcAddress("glDisableVertexAttribArray");
    m_glVertexAttrib4f = (TORC_GLVERTEXATTRIB4FPROC)
        UIOpenGLWindow::GetProcAddress("glVertexAttrib4f");

    bool shaders = m_glShaderSource     && m_glCreateShader &&
                   m_glCompileShader    && m_glGetShaderiv &&
                   m_glGetShaderInfoLog && m_glCreateProgram &&
                   m_glAttachShader     && m_glLinkProgram &&
                   m_glUseProgram       && m_glGetProgramInfoLog &&
                   m_glDetachShader     && m_glGetProgramiv &&
                   m_glDeleteShader     && m_glGetUniformLocation &&
                   m_glUniformMatrix4fv && m_glVertexAttribPointer &&
                   m_glVertexAttrib4f   && m_glEnableVertexAttribArray &&
                   m_glDisableVertexAttribArray && m_glBindAttribLocation;

    // OpenGL 2.0
    bool exts = Extensions.contains("GL_ARB_fragment_shader") &&
                Extensions.contains("GL_ARB_vertex_shader") &&
                Extensions.contains("GL_ARB_shader_objects");

    // available by default in ES 2.0
    m_valid = shaders && (exts || Type == kGLOpenGL2ES);

    // N.B. no implementation should have insufficient texture units
    if (m_valid)
    {
        LOG(VB_GENERAL, LOG_INFO, "GLSL support enabled");
        LOG(VB_GENERAL, LOG_INFO, QString("Max texture units: %1")
                .arg(m_maxTextureUnits));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "No GLSL support");
    }

    return m_valid;
}

uint UIOpenGLShaders::CreateShaderObject(const QByteArray &Vertex, const QByteArray &Fragment)
{
    if (!m_valid)
        return 0;

    uint result = 0;
    QByteArray vert_shader = Vertex.isEmpty() ? kDefaultVertexShader : Vertex;
    QByteArray frag_shader = Fragment.isEmpty() ? kDefaultFragmentShader: Fragment;
    vert_shader.detach();
    frag_shader.detach();

    OptimiseShaderSource(vert_shader);
    OptimiseShaderSource(frag_shader);

    result = m_glCreateProgram();
    if (!result)
        return 0;

    GLShaderObject object(CreateShader(GL_VERTEX_SHADER,   vert_shader),
                          CreateShader(GL_FRAGMENT_SHADER, frag_shader));
    m_shaderObjects.insert(result, object);

    if (!ValidateShaderObject(result))
    {
        DeleteShaderObject(result);
        return 0;
    }

    return result;
}

void UIOpenGLShaders::DeleteShaderObject(uint Object)
{
    if (!m_valid)
        return;
    if (!m_shaderObjects.contains(Object))
        return;

    GLuint vertex   = m_shaderObjects[Object].m_vertexShader;
    GLuint fragment = m_shaderObjects[Object].m_fragmentShader;
    m_glDetachShader(Object, vertex);
    m_glDetachShader(Object, fragment);
    m_glDeleteShader(vertex);
    m_glDeleteShader(fragment);
    m_glDeleteProgram(Object);
    m_shaderObjects.remove(Object);
}

void UIOpenGLShaders::EnableShaderObject(uint Object)
{
    if (!m_valid)
        return;

    if (Object == m_active_obj)
        return;

    if (!Object && m_active_obj)
    {
        m_glUseProgram(0);
        m_active_obj = 0;
        return;
    }

    if (!m_shaderObjects.contains(Object))
        return;

    m_glUseProgram(Object);
    m_active_obj = Object;
}

void UIOpenGLShaders::SetShaderParams(uint Object, GLfloat *Values, const char *Uniform)
{
    if (!m_valid)
        return;

    const GLfloat *v = Values;

    EnableShaderObject(Object);
    GLint loc = m_glGetUniformLocation(Object, Uniform);
    m_glUniformMatrix4fv(loc, 1, GL_FALSE, v);
}

void UIOpenGLShaders::CreateDefaultShaders(void)
{
    if (!m_valid)
        return;

    DeleteDefaultShaders();

    m_shaders[kShaderDefault] = CreateShaderObject(kDefaultVertexShader,
                                                   kDefaultFragmentShader);
    m_shaders[kShaderStudio]  = CreateShaderObject(kDefaultVertexShader,
                                                   kDefaultStudioFragmentShader);
}

void UIOpenGLShaders::DeleteDefaultShaders(void)
{
    if (!m_valid)
        return;

    for (int i = 0; i < kShaderCount; i++)
    {
        DeleteShaderObject(m_shaders[i]);
        m_shaders[i] = 0;
    }
}

uint UIOpenGLShaders::CreateShader(int Type, const QByteArray &Source)
{
    if (!m_valid)
        return 0;

    uint result = m_glCreateShader(Type);
    const char* tmp[1] = { Source.constData() };
    m_glShaderSource(result, 1, tmp, NULL);
    m_glCompileShader(result);
    GLint compiled;
    m_glGetShaderiv(result, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        GLint length = 0;
        m_glGetShaderiv(result, GL_INFO_LOG_LENGTH, &length);
        if (length > 1)
        {
            char *log = (char*)malloc(sizeof(char) * length);
            m_glGetShaderInfoLog(result, length, NULL, log);
            LOG(VB_GENERAL, LOG_ERR, "Failed to compile shader.");
            LOG(VB_GENERAL, LOG_ERR, log);
            LOG(VB_GENERAL, LOG_ERR, Source);
            free(log);
        }
        m_glDeleteShader(result);
        result = 0;
    }
    return result;
}

bool UIOpenGLShaders::ValidateShaderObject(uint Object)
{
    if (!m_valid)
        return false;

    if (!m_shaderObjects.contains(Object))
        return false;
    if (!m_shaderObjects[Object].m_fragmentShader ||
        !m_shaderObjects[Object].m_vertexShader)
        return false;

    m_glAttachShader(Object, m_shaderObjects[Object].m_fragmentShader);
    m_glAttachShader(Object, m_shaderObjects[Object].m_vertexShader);
    m_glBindAttribLocation(Object, VERTEX_INDEX,  "a_position");
    m_glBindAttribLocation(Object, COLOR_INDEX,   "a_color");
    m_glBindAttribLocation(Object, TEXTURE_INDEX, "a_texcoord0");
    m_glLinkProgram(Object);
    return CheckObjectStatus(Object);
}

bool UIOpenGLShaders::CheckObjectStatus(uint Object)
{
    if (!m_valid)
        return false;

    int ok;
    m_glGetProgramiv(Object, GL_OBJECT_LINK_STATUS, &ok);
    if (ok > 0)
        return true;

    LOG(VB_GENERAL, LOG_ERR, "Failed to link shader object.");
    int infologLength = 0;
    int charsWritten  = 0;
    char *infoLog;
    m_glGetProgramiv(Object, GL_OBJECT_INFO_LOG_LENGTH, &infologLength);
    if (infologLength > 0)
    {
        infoLog = (char *)malloc(infologLength);
        m_glGetProgramInfoLog(Object, infologLength, &charsWritten, infoLog);
        LOG(VB_GENERAL, LOG_ERR, QString("\n\n%1").arg(infoLog));
        free(infoLog);
    }
    return false;
}

void UIOpenGLShaders::OptimiseShaderSource(QByteArray &Source)
{
    if (!m_valid)
        return;

    QByteArray extensions = "";
    QByteArray sampler    = "sampler2D";
    QByteArray texture    = "texture2D";

    if (m_usingRects && Source.contains("GLSL_SAMPLER"))
    {
        extensions += "#extension GL_ARB_texture_rectangle : enable\n";
        sampler += "Rect";
        texture += "Rect";
    }

    Source.replace("GLSL_SAMPLER", sampler);
    Source.replace("GLSL_TEXTURE", texture);
    Source.replace("GLSL_DEFINES", m_GLSLVersion + extensions + m_qualifiers);

    LOG(VB_GENERAL, LOG_DEBUG, "\n" + Source);
}

void UIOpenGLShaders::DeleteShaders(void)
{
    if (!m_valid)
        return;

    QHash<GLuint, GLShaderObject>::iterator it;
    for (it = m_shaderObjects.begin(); it != m_shaderObjects.end(); ++it)
    {
        GLuint object   = it.key();
        GLuint vertex   = it.value().m_vertexShader;
        GLuint fragment = it.value().m_fragmentShader;
        m_glDetachShader(object, vertex);
        m_glDetachShader(object, fragment);
        m_glDeleteShader(vertex);
        m_glDeleteShader(fragment);
        m_glDeleteProgram(object);
    }
    m_shaderObjects.clear();
}
