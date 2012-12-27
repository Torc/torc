/* Class UIOpenGLWindow
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

// Qt
#include <QCoreApplication>
#include <QApplication>
#include <QLibrary>
#include <QKeyEvent>
#include <QTime>
#include <QTimer>

// Torc
#include "torclocalcontext.h"
#include "torcevent.h"
#include "torcpower.h"
#include "torclogging.h"
#include "../uitheme.h"
#include "../uiimage.h"
#include "../uifont.h"
#include "../uieffect.h"
#include "../uitimer.h"
#include "uishapepath.h"
#include "uitexteditor.h"
#include "uiopenglwindow.h"

static inline GLenum __glCheck__(const QString &loc, const char* fileName, int n)
{
    GLenum error = glGetError();
    if (error)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("%1: %2 @ %3, %4")
            .arg(loc).arg(error).arg(fileName).arg(n));
    }
    return error;
}

#define glCheck() __glCheck__("OpenGL Error", __FILE__, __LINE__)

UIOpenGLWindow* UIOpenGLWindow::Create(void)
{
    QGLFormat format;
    format.setDepth(false);

    bool setswapinterval = false;

#if defined(Q_OS_MAC)
    LOG(VB_GENERAL, LOG_INFO, "Forcing swap interval for OS X.");
    setswapinterval = true;
#endif

    if (setswapinterval)
        format.setSwapInterval(1);

    return new UIOpenGLWindow(format, kGLOpenGL2ES);
}

#define BLACKLIST QString("MainLoop,close,hide,lower,raise,repaint,setDisabled,setEnabled,setFocus,"\
                          "setHidden,setShown,setStyleSheet,setVisible,setWindowModified,setWindowTitle,"\
                          "show,showFullScreen,showMaximized,showMinimized,showNormal,update,updateGL,updateOverlayGL")

UIOpenGLWindow::UIOpenGLWindow(const QGLFormat &Format, GLType Type)
  : QGLWidget(Format, NULL, NULL, Qt::Window | Qt::MSWindowsOwnDC),
    UIDisplay(this),
    UIWindow(),
    UIImageTracker(),
    UIOpenGLShaders(),
    UIOpenGLFence(),
    UIOpenGLFramebuffers(),
    UIPerformance(),
    TorcHTTPService(this, "/gui", tr("GUI"), UIOpenGLWindow::staticMetaObject, BLACKLIST),
    m_timer(NULL),
    m_openGLType(Type),
    m_blend(false),
    m_backgroundColour(0x00000000)
{
    setObjectName("MainWindow");

    TorcReferenceCounter::EventLoopEnding(false);

    gLocalContext->SetUIObject(this);
    gLocalContext->AddObserver(this);

    //installEventFilter(this);
    setAutoBufferSwap(false);

    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
    setWindowState(Qt::WindowFullScreen);

    InitialiseWindow();
    InitialiseDisplay();

    setGeometry(0, 0, m_pixelSize.width(), m_pixelSize.height());
    setFixedSize(m_pixelSize);

    raise();
    show();

    setCursor(Qt::BlankCursor);
    grabKeyboard();

    SetRefreshRate(m_refreshRate);
    m_mainTimer = startTimer(0);
}

UIOpenGLWindow::~UIOpenGLWindow()
{
    // ensure QObjects are deleted immediately
    TorcReferenceCounter::EventLoopEnding(true);

    // stop display timer
    if (m_mainTimer)
        killTimer(m_mainTimer);
    m_mainTimer = 0;

    // delete the performance monitor
    if (m_timer)
        delete m_timer;
    m_timer = NULL;

    // stop listening
    gLocalContext->RemoveObserver(this);
    gLocalContext->SetUIObject(NULL);

    // delete the theme BEFORE UIImageTracker is destroyed
    if (m_theme)
        m_theme->DownRef();
    m_theme = NULL;

    // free up OpenGL textures
    ReleaseAllTextures();
}

QGLFunctionPointer UIOpenGLWindow::GetProcAddress(const QString &Proc)
{
    static const QString exts[4] = { "", "ARB", "EXT", "OES" };

    QGLFunctionPointer result = NULL;

    for (int i = 0; i < 4; i++)
    {
        if (QGLFormat::openGLVersionFlags() & QGLFormat::OpenGL_ES_Version_2_0)
            result = (QGLFunctionPointer)QLibrary::resolve("GLESv2", QString(Proc + exts[i]).toLatin1().data());
        else
            result = (QGLFunctionPointer)QGLContext::currentContext()->getProcAddress(Proc + exts[i]);

        if (result)
            break;
    }

    if (result == NULL)
    {
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("OpenGLProc: Extension not found: %1").arg(Proc));
    }

    return result;
}

void UIOpenGLWindow::MainLoop(void)
{
    quint64 timestamp = StartFrame();

    makeCurrent();
    SetViewPort(geometry());
    ClearFramebuffer();

    CheckForNewTheme();
    UpdateImages();

    if (m_theme)
    {
        m_theme->Refresh(timestamp);
        m_theme->Draw(timestamp, this, 0.0, 0.0);
    }

    FinishDrawing();

    if (m_timer)
    {
        m_timer->Wait();
        m_timer->Start();
    }

    if (isVisible())
        swapBuffers();
}

QVariantMap UIOpenGLWindow::GetDisplayDetails(void)
{
    QVariantMap result;
    QSize mm = GetPhysicalSize();
    QSize pixels = GetSize();

    result.insert("refreshrate", GetRefreshRate());
    result.insert("widthmm", mm.width());
    result.insert("heightmm", mm.height());
    result.insert("aspectratio", (float)mm.width() / (float)mm.height());
    result.insert("widthpixels", pixels.width());
    result.insert("heightpixels", pixels.height());
    result.insert("renderer", m_openGLType == kGLOpenGL2 ? "OpenGL2.0" : kGLOpenGL2ES ? "OpenGLES2.0" : "Unknown");
    result.insert("studiolevels", m_studioLevels);
    return result;
}

QVariantMap UIOpenGLWindow::GetThemeDetails(void)
{
    QMutexLocker locker(m_newThemeLock);

    if (m_theme)
        return m_theme->ToMap();

    return QVariantMap();
}

void UIOpenGLWindow::InitialiseWindow(void)
{
    makeCurrent();

    if ((kGLOpenGL2ES == m_openGLType) &&
        !(QGLFormat::openGLVersionFlags() & QGLFormat::OpenGL_ES_Version_2_0))
    {
        m_openGLType = kGLOpenGL2;
    }

    if (m_openGLType == kGLOpenGL2ES)
        LOG(VB_GENERAL, LOG_INFO, "Using OpenGL ES 2.0");

    m_extensions = (reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS)));

    LOG(VB_GENERAL, LOG_INFO, QString("OpenGL vendor  : %1")
            .arg((const char*) glGetString(GL_VENDOR)));
    LOG(VB_GENERAL, LOG_INFO, QString("OpenGL renderer: %1")
            .arg((const char*) glGetString(GL_RENDERER)));
    LOG(VB_GENERAL, LOG_INFO, QString("OpenGL version : %1")
            .arg((const char*) glGetString(GL_VERSION)));
    LOG(VB_GENERAL, LOG_INFO, QString("Direct rendering: %1")
            .arg((QGLContext::currentContext()->format().directRendering()) ? "Yes" : "No"));
    LOG(VB_GENERAL, LOG_INFO, QString("Multisampling: %1 (samples %2)")
            .arg(QGLContext::currentContext()->format().sampleBuffers() ? "Yes" : "No")
            .arg(QGLContext::currentContext()->format().samples()));

    LOG(VB_GENERAL, LOG_DEBUG, m_extensions);

    SetBlend(false);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glClear(GL_COLOR_BUFFER_BIT);

    InitialiseView(geometry());
    InitialiseTextures(m_extensions, m_openGLType);
    InitialiseShaders(m_extensions, m_openGLType, IsRectTexture((uint)m_defaultTextureType));
    InitialiseFence(m_extensions);
    InitialiseFramebuffers(m_extensions, m_openGLType);
    CreateDefaultShaders();
    Flush(true);

    LOG(VB_GENERAL, LOG_INFO, "Initialised UIOpenGLWindow");

    doneCurrent();
}

bool UIOpenGLWindow::eventFilter(QObject *Object, QEvent *Event)
{
    return false;
}

void UIOpenGLWindow::customEvent(QEvent *Event)
{
    if (Event->type() == TorcEvent::TorcEventType)
    {
        TorcEvent* torcevent = dynamic_cast<TorcEvent*>(Event);
        if (!torcevent)
            return;

        int event = torcevent->Event();

        switch (event)
        {
            case Torc::Exit:
                close();
                break;
            default:
                break;
        }
    }
}

void UIOpenGLWindow::closeEvent(QCloseEvent *Event)
{
    LOG(VB_GENERAL, LOG_INFO, "Closing window");
    QWidget::closeEvent(Event);
}

bool UIOpenGLWindow::event(QEvent *Event)
{
    int type = Event->type();

    if (type == QEvent::KeyPress || type == QEvent::KeyRelease)
    {
        QKeyEvent *keyevent = static_cast<QKeyEvent*>(Event);

        if (keyevent)
        {
            keyevent->accept();
            UIWidget* focuswidget = m_theme ? m_theme->GetFocusWidget() : NULL;

            if (focuswidget && focuswidget->Type() == UITextEditor::kUITextEditorType)
            {
                UITextEditor *texteditor = static_cast<UITextEditor*>(focuswidget);
                if (texteditor && texteditor->HandleTextInput(keyevent))
                    return true;
            }

            int action = GetActionFromKey(keyevent);

            if (action && focuswidget)
            {
                if (focuswidget->HandleAction(action))
                    return true;
            }

            switch (action)
            {
                case Torc::Escape:
                    close();
                    return true;
                case Torc::Suspend:
                    gPower->Suspend();
                    return true;
                case Torc::DisableStudioLevels:
                    SetStudioLevels(false);
                    break;
                case Torc::EnableStudioLevels:
                    SetStudioLevels(true);
                    break;
                case Torc::ToggleStudioLevels:
                    SetStudioLevels(!m_studioLevels);
                    break;
                default: break;
            }
        }
    }
    else if (type == QEvent::Timer)
    {
        QTimerEvent *timerevent = dynamic_cast<QTimerEvent*>(Event);

        if (timerevent && timerevent->timerId() == m_mainTimer)
        {
            MainLoop();
            return true;
        }
    }

    return QGLWidget::event(Event);
}

QSize UIOpenGLWindow::GetSize(void)
{
    return m_pixelSize; // NB Screen size
}

void UIOpenGLWindow::SetRefreshRate(double Rate, int ModeIndex)
{
    if (m_timer && qFuzzyCompare(Rate + 1.0f, m_refreshRate + 1.0f))
        return;

    m_refreshRate = Rate;
    if (ModeIndex > -1)
        SwitchToMode(ModeIndex); // NB this will also update m_refreshRate

    LOG(VB_GENERAL, LOG_INFO, QString("Setting display rate to %1 (interval %2us)")
        .arg(m_refreshRate).arg(1000000.0f / m_refreshRate));

    SetFrameCount(5000.0 / m_refreshRate);

    if (m_timer)
        m_timer->SetInterval(1000000.0f / m_refreshRate);
    else
        m_timer = new UITimer(1000000.0f / m_refreshRate);
}

void UIOpenGLWindow::DrawImage(UIEffect *Effect,
                               QRectF  *Dest,     bool     &PositionChanged,
                               UIImage *Image)
{
    if (!Image)
        return;

    if (Image->GetState() == UIImage::ImageReleasedFromGPU)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Image '%1' cache hit").arg(Image->GetName()));
        Image->SetState(UIImage::ImageNull);
    }

    if (Image->GetState() == UIImage::ImageLoading)
        return;

    if (Image->GetState() == UIImage::ImageNull)
    {
        LoadImageFromFile(Image);
        return;
    }

    if (!Dest)
        return;

    PositionChanged |= Image->IsShared();

    GLTexture *texture = AllocateTexture(Image);
    if (texture)
        DrawTexture(texture, Dest, Image->GetSizeF(), PositionChanged);
}

void UIOpenGLWindow::DrawTexture(GLTexture *Texture, QRectF *Dest, QSizeF *Size, uint Shader)
{
    SetShaderParams(Shader, &m_currentProjection->m[0][0], "u_projection");
    SetShaderParams(Shader, &m_transforms[m_currentTransformIndex].m[0][0], "u_transform");

    glBindTexture(Texture->m_type, Texture->m_val);

    m_glBindBuffer(GL_ARRAY_BUFFER, Texture->m_vbo);

    if (!Texture->m_vboUpdated)
    {
        UpdateTextureVertices(Texture, Size, Dest);
        if (m_mapBuffers)
        {
            m_glBufferData(GL_ARRAY_BUFFER, kVertexSize, NULL, GL_DYNAMIC_DRAW);
            void* buf = m_glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
            if (buf)
                memcpy(buf, Texture->m_vertexData, kVertexSize);
            m_glUnmapBuffer(GL_ARRAY_BUFFER);
        }
        else
        {
            m_glBufferData(GL_ARRAY_BUFFER, kVertexSize, Texture->m_vertexData, GL_DYNAMIC_DRAW);
        }
    }

    m_glEnableVertexAttribArray(VERTEX_INDEX);
    m_glEnableVertexAttribArray(TEXTURE_INDEX);

    m_glVertexAttribPointer(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                            VERTEX_SIZE * sizeof(GLfloat),
                            (const void *) kVertexOffset);
    m_glVertexAttrib4f(COLOR_INDEX, 1.0, 1.0, 0.0, m_transforms[m_currentTransformIndex].alpha);
    m_glVertexAttribPointer(TEXTURE_INDEX, TEXTURE_SIZE, GL_FLOAT, GL_FALSE,
                            TEXTURE_SIZE * sizeof(GLfloat),
                            (const void *) kTextureOffset);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    m_glDisableVertexAttribArray(TEXTURE_INDEX);
    m_glDisableVertexAttribArray(VERTEX_INDEX);
    m_glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void UIOpenGLWindow::DrawTexture(GLTexture *Texture, QRectF *Dest, QSizeF *Size, bool &PositionChanged)
{
    static const GLfloat studiorange  = 219.0f / 255.0f;
    static const GLfloat studiooffset = 16.0f / 255.0f;
    uint ShaderObject = m_shaders[kShaderDefault];

    BindFramebuffer(0);

    EnableShaderObject(ShaderObject);
    SetShaderParams(ShaderObject, &m_currentProjection->m[0][0], "u_projection");
    SetShaderParams(ShaderObject, &m_transforms[m_currentTransformIndex].m[0][0], "u_transform");
    SetBlend(true);

    glBindTexture(Texture->m_type, Texture->m_val);

    m_glBindBuffer(GL_ARRAY_BUFFER, Texture->m_vbo);

    if (PositionChanged || !Texture->m_vboUpdated)
    {
        UpdateTextureVertices(Texture, Size, Dest);
        if (m_mapBuffers)
        {
            m_glBufferData(GL_ARRAY_BUFFER, kVertexSize, NULL, GL_DYNAMIC_DRAW);
            void* buf = m_glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
            if (buf)
                memcpy(buf, Texture->m_vertexData, kVertexSize);
            m_glUnmapBuffer(GL_ARRAY_BUFFER);
        }
        else
        {
            m_glBufferData(GL_ARRAY_BUFFER, kVertexSize, Texture->m_vertexData, GL_DYNAMIC_DRAW);
        }
        PositionChanged = false;
    }

    m_glEnableVertexAttribArray(VERTEX_INDEX);
    m_glEnableVertexAttribArray(TEXTURE_INDEX);

    m_glVertexAttribPointer(VERTEX_INDEX, VERTEX_SIZE, GL_FLOAT, GL_FALSE,
                            VERTEX_SIZE * sizeof(GLfloat),
                            (const void *) kVertexOffset);
    m_glVertexAttrib4f(COLOR_INDEX,
                       m_transforms[m_currentTransformIndex].color,
                       m_studioLevels ? studiorange : 1.0f,
                       m_studioLevels ? studiooffset : 0.0f,
                       m_transforms[m_currentTransformIndex].alpha);
    m_glVertexAttribPointer(TEXTURE_INDEX, TEXTURE_SIZE, GL_FLOAT, GL_FALSE,
                            TEXTURE_SIZE * sizeof(GLfloat),
                            (const void *) kTextureOffset);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    m_glDisableVertexAttribArray(TEXTURE_INDEX);
    m_glDisableVertexAttribArray(VERTEX_INDEX);
    m_glBindBuffer(GL_ARRAY_BUFFER, 0);
}

UIImage* UIOpenGLWindow::DrawText(UIEffect *Effect,
                                  QRectF *Dest, bool &PositionChanged,
                                  const QString &Text, UIFont *Font, int Flags,
                                  int Blur, UIImage *Fallback)
{
    if (!Font->GetImageReady())
    {
        // font is image backed
        QString file = Font->GetImageFileName();
        UIImage *image = Font->GetImage();

        // allocate an image
        if (!image)
        {
            QString name = Font->GetHash() + file;
            QSize size(Dest->size().width(), Dest->size().height());
            image = AllocateImage(name, size, file);
            Font->SetImage(image);
        }

        if (!image)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to load '%1' text image.")
                    .arg(file));
            // don't try again
            Font->SetImageReady(true);
            return NULL;
        }

        UIImage::ImageState state = image->GetState();

        if (state == UIImage::ImageUploadedToGPU ||
            state == UIImage::ImageReleasedFromGPU)
        {
            LOG(VB_GENERAL, LOG_ERR, "Font image cannot be unloaded from memory");
            Font->SetImageReady(true);
            return NULL;
        }

        if (state == UIImage::ImageLoading)
            return NULL;

        // trigger loading
        if (state == UIImage::ImageNull)
        {
            LoadImageFromFile(image);
            return NULL;
        }

        // update the font's brush
        Font->UpdateTexture();
    }

    UIImage* image = GetSimpleTextImage(Text, Dest, Font, Flags, Blur);

    if (!image)
        return NULL;

    if ((image->GetState() == UIImage::ImageLoading) && Fallback)
        image = Fallback;

    DrawImage(Effect, Dest, PositionChanged, image);

    return image;
}

void UIOpenGLWindow::DrawShape(UIEffect *Effect,
                               QRectF *Dest, bool &PositionChanged,
                               UIShapePath *Path)
{
    if (!Dest || !Path)
        return;

    UIImage* image = GetShapeImage(Path, Dest);

    if (!image)
        return;

    if (image->GetState() == UIImage::ImageLoading)
        return;

    PositionChanged |= Path->IsShared();

    DrawImage(Effect, Dest, PositionChanged, image);
}

bool UIOpenGLWindow::PushEffect(const UIEffect *Effect, const QRectF *Dest)
{
    return PushTransformation(Effect, Dest);
}

void UIOpenGLWindow::PopEffect(void)
{
    PopTransformation();
}

void UIOpenGLWindow::PushClip(const QRect &Rect)
{
    PushClipRect(Rect);
}

void UIOpenGLWindow::PopClip(void)
{
    PopClipRect();
}

GLTexture* UIOpenGLWindow::AllocateTexture(UIImage *Image)
{
    if (!Image)
        return NULL;

    if (m_textureHash.contains(Image))
    {
        m_textureExpireList.removeOne(Image);
        m_textureExpireList.append(Image);
        return m_textureHash[Image];
    }

    QImage glimage     = QGLWidget::convertToGLFormat(*Image);
    GLTexture *texture = CreateTexture(glimage.size(), true, 0,
                                       GL_UNSIGNED_BYTE, GL_RGBA, GL_RGBA8,
                                       GL_LINEAR_MIPMAP_LINEAR);

    if (!texture)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to create OpenGL texture.");
        return NULL;
    }

    m_hardwareCacheSize += texture->m_internalDataSize;
    void* buffer = GetTextureBuffer(texture);
    if (buffer)
        memcpy(buffer, glimage.bits(), texture->m_dataSize);
    UpdateTexture(texture, glimage.bits());

    m_textureHash[Image] = texture;
    m_textureExpireList.append(Image);

    if (Image->GetState() == UIImage::ImageLoaded)
    {
        m_softwareCacheSize -= Image->byteCount();
        Image->FreeMem();
    }

    while ((m_hardwareCacheSize > m_maxHardwareCacheSize) &&
           m_textureExpireList.size())
    {
        ReleaseTexture(m_textureExpireList.takeFirst());
    }

    return texture;
}

void UIOpenGLWindow::ReleaseTexture(UIImage *Image)
{
    if (!m_textureHash.contains(Image))
        return;

    Image->SetState(UIImage::ImageReleasedFromGPU);

    m_textureExpireList.removeAll(Image);
    GLTexture *texture   = m_textureHash.take(Image);
    m_hardwareCacheSize -= texture->m_internalDataSize;
    DeleteTexture(texture->m_val);
}

void UIOpenGLWindow::ReleaseAllTextures(void)
{
    while (!m_textureExpireList.isEmpty())
        ReleaseTexture(m_textureExpireList.takeFirst());
}

void UIOpenGLWindow::SetBlend(bool Enable)
{
    if (Enable && !m_blend)
        glEnable(GL_BLEND);
    else if (!Enable && m_blend)
        glDisable(GL_BLEND);
    m_blend = Enable;
}

void UIOpenGLWindow::SetBackground(quint8 Red, quint8 Green, quint8 Blue, quint8 Alpha)
{
    uint32_t background = (Red << 24) + (Green << 16) + (Blue << 8) + Alpha;
    if (background == m_backgroundColour)
        return;

    m_backgroundColour = background;
    glClearColor(Red / 255.0, Green / 255.0, Blue / 255.0, Alpha / 255.0);
}
