/* Class UIDirect3D9Window
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2013
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
#include <QApplication>
#include <QLibrary>
#include <QKeyEvent>

// Torc

#include "torclocalcontext.h"
#include "torcevent.h"
#include "torcpower.h"
#include "torclogging.h"
#include "../uitheme.h"
#include "../uiimage.h"
#include "../uifont.h"
#include "../uieffect.h"
#include "../uishapepath.h"
#include "../uitexteditor.h"
#include "../uitimer.h"
#include "uidirect3d9window.h"

typedef LPDIRECT3D9 (WINAPI *LPFND3DC)(UINT SDKVersion);

#define BLACKLIST QString("MainLoop,close,hide,lower,raise,repaint,setDisabled,setEnabled,setFocus,"\
                          "setHidden,setShown,setStyleSheet,setVisible,setWindowModified,setWindowTitle,"\
                          "show,showFullScreen,showMaximized,showMinimized,showNormal,update")

UIDirect3D9Window* UIDirect3D9Window::Create(void)
{
    UIDirect3D9Window* window = new UIDirect3D9Window();
    if (window && window->Initialise())
        return window;

    delete window;
    return NULL;
}

UIDirect3D9Window::UIDirect3D9Window()
  : QWidget(NULL, Qt::Window | Qt::MSWindowsOwnDC),
    UIDirect3D9View(),
    UIDirect3D9Textures(),
    UIDirect3D9Shaders(),
    UIDisplay(this),
    UIWindow(),
    UIImageTracker(),
    UIPerformance(),
    UIActions(),
    TorcHTTPService(this, "/gui", tr("GUI"), UIDirect3D9Window::staticMetaObject, BLACKLIST),
    m_timer(NULL),
    m_d3dObject(NULL),
    m_d3dDevice(NULL),
    m_deviceState(D3D_OK),
    m_defaultRenderTarget(NULL),
    m_currentRenderTarget(NULL),
    m_blend(false)
{
}

UIDirect3D9Window::~UIDirect3D9Window()
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

    // teardown d3d
    Destroy();
}

bool UIDirect3D9Window::Initialise(void)
{
    setObjectName("MainWindow");

    TorcReferenceCounter::EventLoopEnding(false);

    gLocalContext->SetUIObject(this);
    gLocalContext->AddObserver(this);

    show();

    InitialiseDisplay();

    setGeometry(0, 0, m_pixelSize.width(), m_pixelSize.height());
    setFixedSize(m_pixelSize);

    raise();

    setCursor(Qt::BlankCursor);
    grabKeyboard();

    SetRefreshRate(m_refreshRate);
    m_mainTimer = startTimer(0);

    return InitialiseDirect3D9();
}

void* UIDirect3D9Window::GetD3DX9ProcAddress(const QString &Proc)
{
    static bool libraryChecked = false;
    static QLibrary *library   = new QLibrary();

    if (!libraryChecked)
    {
        libraryChecked = true;
        for (int i = 43; i > 24; --i)
        {
            QString libname = QString("d3dx9_%1").arg(i);
            library->setFileName(libname);
            if (library->load())
            {
                LOG(VB_GENERAL, LOG_INFO, "Loaded " + libname);
                break;
            }
            else
            {
                LOG(VB_GENERAL, LOG_INFO, "Failed to find " + libname);
            }
        }

        if (!library->isLoaded())
        {
            LOG(VB_GENERAL, LOG_ERR,
                "!!! Failed to find any D3Dx9 library - make sure you have the "
                "DirectX runtime installed !!!");
        }
    }

    if(library->isLoaded())
    {
        void* result = library->resolve(Proc.toLatin1().data());
        if (result)
            return result;
    }

    LOG(VB_GENERAL, LOG_ERR, QString("Function address not found: '%1'").arg(Proc));
    return NULL;
}

bool UIDirect3D9Window::InitialiseDirect3D9(void)
{
    static LPFND3DC direct3Dcreate9 = NULL;

    // retrieve function pointer
    if (!direct3Dcreate9)
    {
        direct3Dcreate9 = (LPFND3DC)QLibrary::resolve("d3d9", "Direct3DCreate9");

        if (!direct3Dcreate9)
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to find Direct3D9 library");
            return false;
        }
    }

    // create D3D object
    m_d3dObject = direct3Dcreate9(D3D_SDK_VERSION);
    if (!m_d3dObject)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to created D3D9 object");
        return false;
    }

    LOG(VB_GENERAL, LOG_INFO, "Created D3D9 object");

    // determine the adaptor
    unsigned int adaptor = GetAdapterNumber();

    // retrieve the device capabilities
    D3DCAPS9 capabilities;
    memset(&capabilities, 0, sizeof(capabilities));
    m_d3dObject->GetDeviceCaps(adaptor, D3DDEVTYPE_HAL, &capabilities);

    m_adaptorFormat = D3DFMT_X8R8G8B8;
    D3DDISPLAYMODE d3ddm;
    if (D3D_OK == m_d3dObject->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm))
        m_adaptorFormat = d3ddm.Format;

    // hardware/software vertex processing
    DWORD vertexflag = capabilities.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ?
                D3DCREATE_HARDWARE_VERTEXPROCESSING : D3DCREATE_SOFTWARE_VERTEXPROCESSING;

    // determine presentation parameters
    D3DPRESENT_PARAMETERS parameters;
    GetPresentParameters(parameters);

    // create the device
    HRESULT result = m_d3dObject->CreateDevice(adaptor, D3DDEVTYPE_HAL, winId(),
                                               vertexflag | D3DCREATE_MULTITHREADED,
                                               &parameters, &m_d3dDevice);

    if (result != D3D_OK)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to create D3D9 device");
        return false;
    }

    D3DADAPTER_IDENTIFIER9 identifier;
    if (D3D_OK == m_d3dObject->GetAdapterIdentifier(adaptor, 0, &identifier))
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Device  : %1").arg(identifier.Description));
        LOG(VB_GENERAL, LOG_INFO, QString("Driver  : %1").arg(identifier.Driver));;
        LOG(VB_GENERAL, LOG_INFO, QString("Version : %1.%2.%3.%4")
            .arg(HIWORD(identifier.DriverVersion.HighPart))
            .arg(LOWORD(identifier.DriverVersion.HighPart))
            .arg(HIWORD(identifier.DriverVersion.LowPart))
            .arg(LOWORD(identifier.DriverVersion.LowPart)));
        LOG(VB_GUI, LOG_INFO, QString("VendorID: 0x%1").arg(identifier.VendorId, 0, 16));
        LOG(VB_GUI, LOG_INFO, QString("DeviceID: 0x%1").arg(identifier.DeviceId, 0, 16));
    }

    LOG(VB_GENERAL, LOG_INFO, "Created D3D9 device");

    return Initialise2DState() &&
           InitialiseView(m_d3dDevice, geometry()) &&
           InitialiseTextures(m_d3dObject, m_d3dDevice, adaptor, m_adaptorFormat) &&
           InitialiseShaders(m_d3dDevice);
}

bool UIDirect3D9Window::Initialise2DState(void)
{
    LOG(VB_GENERAL, LOG_INFO, "Initialising 2D state");

    m_d3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    m_d3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    m_d3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    m_d3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

    m_d3dDevice->SetRenderState(D3DRS_AMBIENT,       D3DCOLOR_XRGB(255,255,255));
    m_d3dDevice->SetRenderState(D3DRS_CULLMODE,      D3DCULL_NONE);
    m_d3dDevice->SetRenderState(D3DRS_ZENABLE,       D3DZB_FALSE);
    m_d3dDevice->SetRenderState(D3DRS_LIGHTING,      FALSE);
    m_d3dDevice->SetRenderState(D3DRS_DITHERENABLE,  TRUE);
    m_d3dDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    m_d3dDevice->SetRenderState(D3DRS_SRCBLEND,      D3DBLEND_SRCALPHA);
    m_d3dDevice->SetRenderState(D3DRS_DESTBLEND,     D3DBLEND_INVSRCALPHA);

    SetBlend(true);

    if (FAILED(m_d3dDevice->GetRenderTarget(0, &m_defaultRenderTarget)))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to get default render target");
        return false;
    }

    return true;
}

void UIDirect3D9Window::Destroy(void)
{
    LOG(VB_GENERAL, LOG_INFO, "Destroying D3D9 objects");

    if (m_currentRenderTarget)
        m_currentRenderTarget->Release();

    if (m_defaultRenderTarget)
        m_defaultRenderTarget->Release();

    ReleaseAllTextures();

    if (m_d3dDevice)
        m_d3dDevice->Release();
    m_d3dDevice = NULL;

    if (m_d3dObject)
        m_d3dObject->Release();
    m_d3dObject = NULL;
}

D3D9Texture* UIDirect3D9Window::AllocateTexture(UIImage *Image)
{
    if (!Image || !m_d3dDevice)
        return NULL;

    if (m_textureHash.contains(Image))
    {
        m_textureExpireList.removeOne(Image);
        m_textureExpireList.append(Image);
        return m_textureHash[Image];
    }

    D3D9Texture *texture = CreateTexture(m_d3dDevice, Image->size());

    if (!texture)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to create Direct3D9 texture.");
        return NULL;
    }

    m_hardwareCacheSize += texture->m_internalDataSize;
    UpdateTexture(m_d3dDevice, texture, Image);

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

void UIDirect3D9Window::ReleaseTexture(UIImage *Image)
{
    if (!m_textureHash.contains(Image))
        return;

    Image->SetState(UIImage::ImageReleasedFromGPU);

    m_textureExpireList.removeAll(Image);
    D3D9Texture *texture = m_textureHash.take(Image);
    m_hardwareCacheSize -= texture->m_internalDataSize;
    DeleteTexture(texture);
}


void UIDirect3D9Window::ReleaseAllTextures(void)
{
    while (!m_textureExpireList.isEmpty())
        ReleaseTexture(m_textureExpireList.takeFirst());
}

void UIDirect3D9Window::customEvent(QEvent *Event)
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

void UIDirect3D9Window::closeEvent(QCloseEvent *Event)
{
    LOG(VB_GENERAL, LOG_INFO, "Closing window");
    QWidget::closeEvent(Event);
}

bool UIDirect3D9Window::event(QEvent *Event)
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
    else if (type == QEvent::Paint)
    {
        return true;
    }

    return QWidget::event(Event);
}

bool UIDirect3D9Window::winEvent(MSG *Message, long *Result)
{
    // TODO other messages worth handling
    // WM_MEDIA_CHANGE
    // WM_DEVICECHANGE
    // WM_PAINT
    // WM_QUERYENDSESSION
    // WM_SYSCOMMAND
    if (Message)
    {
        // see comments in libs/libtorc-core/platforms/torcpowerwin.cpp
        if (Message->message == WM_POWERBROADCAST)
        {
            // we really only want to handle this once and these two events
            // should cover all eventualities - hence we do not explicitly
            // handle PBT_APMRESUMESUSPEND or PBT_APMRESUMESTANDBY
            if (Message->wParam == PBT_APMRESUMEAUTOMATIC ||
                Message->wParam == PBT_APMRESUMECRITICAL)
            {
                gPower->WokeUp();
            }
            else if (Message->wParam == PBT_APMSUSPEND ||
                     Message->wParam == PBT_APMSTANDBY)
            {
                gPower->Suspending();
            }
            else if (Message->wParam == PBT_APMQUERYSTANDBYFAILED ||
                     Message->wParam == PBT_APMQUERYSUSPENDFAILED)
            {
                // TODO this may need to be handled properly
                LOG(VB_GENERAL, LOG_WARNING, "System suspension failed");
            }
            else if (Message->wParam == PBT_APMQUERYSTANDBY ||
                     Message->wParam == PBT_APMQUERYSUSPEND)
            {
                LOG(VB_GENERAL, LOG_INFO, "Received suspend/standby query");
            }
        }
        else if (Message->message == WM_ENDSESSION)
        {
            // TODO not received
            LOG(VB_GENERAL, LOG_INFO, "Received 'ENDSESSION' event");
            close();
        }
        else if (Message->message == WM_QUERYENDSESSION)
        {
            // TODO not received
            LOG(VB_GENERAL, LOG_INFO, "Received 'QUERYENDSESSION' event");
        }
    }

    return false;
}

void UIDirect3D9Window::MainLoop(void)
{
    quint64 timestamp = StartFrame();

    CheckForNewTheme();
    UpdateImages();

    if (m_d3dDevice)
    {
        HRESULT result = m_d3dDevice->TestCooperativeLevel();

        if (SUCCEEDED(result))
        {
            m_d3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);

            if (m_theme && m_d3dDevice && SUCCEEDED(m_d3dDevice->BeginScene()))
            {
                m_theme->Refresh(timestamp);
                m_theme->Draw(timestamp, this, 0.0, 0.0);

                m_d3dDevice->EndScene();
                m_d3dDevice->Present(NULL, NULL, NULL, NULL);
                SetThreadExecutionState(ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
            }
        }
        else
        {
            switch (result)
            {
                case D3DERR_DEVICELOST:
                    if (D3DERR_DEVICELOST != m_deviceState)
                        LOG(VB_GENERAL, LOG_ERR, "Device lost");
                    m_deviceState = D3DERR_DEVICELOST;
                    break;
                case D3DERR_DRIVERINTERNALERROR:
                    LOG(VB_GENERAL, LOG_ERR, "Driver error - quiting");
                    m_deviceState = D3DERR_DRIVERINTERNALERROR;
                    qApp->exit();
                    break;
                case D3DERR_DEVICENOTRESET:
                    if (D3DERR_DEVICENOTRESET != m_deviceState)
                        LOG(VB_GENERAL, LOG_ERR, "Device needs reset");
                     m_deviceState = D3DERR_DEVICENOTRESET;
                     HandleDeviceReset();
                default:
                    break;
            }
        }
    }

    FinishDrawing();

    if (m_timer)
    {
        m_timer->Wait();
        m_timer->Start();
    }
}

void UIDirect3D9Window::GetPresentParameters(D3DPRESENT_PARAMETERS &Parameters)
{
    memset(&Parameters, 0, sizeof(D3DPRESENT_PARAMETERS));
    Parameters.BackBufferFormat     = m_adaptorFormat;
    Parameters.hDeviceWindow        = winId();
    Parameters.Windowed             = true;
    Parameters.BackBufferWidth      = m_pixelSize.width();
    Parameters.BackBufferHeight     = m_pixelSize.height();
    Parameters.BackBufferCount      = 1;
    Parameters.MultiSampleType      = D3DMULTISAMPLE_NONE;
    Parameters.SwapEffect           = D3DSWAPEFFECT_DISCARD;
    Parameters.Flags                = D3DPRESENTFLAG_VIDEO;
    Parameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
}

unsigned int UIDirect3D9Window::GetAdapterNumber(void)
{
    unsigned int adapter = D3DADAPTER_DEFAULT;
    HMONITOR monitor = MonitorFromWindow(winId(), MONITOR_DEFAULTTONULL);
    if (monitor)
    {
        for (unsigned int i = 0; i < m_d3dObject->GetAdapterCount(); ++i)
        {
            HMONITOR mon = m_d3dObject->GetAdapterMonitor(i);
            if (mon == monitor)
            {
                adapter = i;
                break;
            }
        }
    }

    return adapter;
}

void UIDirect3D9Window::HandleDeviceReset(void)
{
    D3DPRESENT_PARAMETERS parameters;
    GetPresentParameters(parameters);

    {
        HRESULT reset = m_d3dDevice->Reset(&parameters);
        switch (reset)
        {
            case D3D_OK:
                LOG(VB_GENERAL, LOG_INFO, "Device successfully reset");

                m_deviceState = D3D_OK;

                // force some state changes
                m_blend = !m_blend;
                if (m_defaultRenderTarget)
                {
                    m_defaultRenderTarget->Release();
                    m_defaultRenderTarget = NULL;
                }

                Initialise2DState();
                InitialiseTextures(m_d3dObject, m_d3dDevice, GetAdapterNumber(), m_adaptorFormat);
                InitialiseView(m_d3dDevice, geometry());
                InitialiseShaders(m_d3dDevice);
                break;
            case D3DERR_DEVICENOTRESET:
                LOG(VB_GUI, LOG_INFO, "Device still not reset");
                break;
            case D3DERR_DEVICELOST:
                LOG(VB_GUI, LOG_INFO, "Device lost");
                break;
            case D3DERR_DRIVERINTERNALERROR:
                LOG(VB_GUI, LOG_INFO, "Driver error");
                break;
            case D3DERR_OUTOFVIDEOMEMORY:
                LOG(VB_GUI, LOG_INFO, "Out of video memory");
                break;
            case D3DERR_INVALIDDEVICE:
                LOG(VB_GUI, LOG_INFO, "Invalid device");
                break;
            case D3DERR_INVALIDCALL:
                LOG(VB_GUI, LOG_INFO, "Invalid call");
                break;
            case D3DERR_DRIVERINVALIDCALL:
                LOG(VB_GUI, LOG_INFO, "Driver invalid call");
                break;
            case D3DERR_NOTAVAILABLE:
                LOG(VB_GUI, LOG_INFO, "Not available");
                break;
            default:
                LOG(VB_GUI, LOG_INFO, QString("Unknown Reset result %1").arg(reset));
        }

        if (FAILED(reset))
        {
            LOG(VB_GENERAL, LOG_INFO, "Releasing all direct3d resources");

            if (m_defaultRenderTarget)
            {
                m_defaultRenderTarget->Release();
                m_defaultRenderTarget = NULL;
            }

            if (m_currentRenderTarget)
            {
                m_currentRenderTarget->Release();
                m_currentRenderTarget = NULL;
            }

            ReleaseAllTextures();

            gLocalContext->NotifyEvent(Torc::DisplayDeviceReset);
        }
    }
}

QVariantMap UIDirect3D9Window::GetDisplayDetails(void)
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
    result.insert("renderer", "Direct3D9");
    result.insert("studiolevels", m_studioLevels);
    return result;
}

QVariantMap UIDirect3D9Window::GetThemeDetails(void)
{
    QMutexLocker locker(m_newThemeLock);

    if (m_theme)
        return m_theme->ToMap();

    return QVariantMap();
}

QSize UIDirect3D9Window::GetSize(void)
{
    return m_pixelSize;
}

void UIDirect3D9Window::SetRefreshRate(double Rate, int ModeIndex)
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

UIImage* UIDirect3D9Window::DrawText(UIEffect *Effect, QRectF *Dest, bool &PositionChanged, const QString &Text, UIFont *Font, int Flags, int Blur, UIImage *Fallback)
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

void UIDirect3D9Window::DrawImage(UIEffect *Effect, QRectF *Dest, bool &PositionChanged, UIImage *Image)
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

    D3D9Texture *texture = AllocateTexture(Image);
    if (texture)
        DrawTexture(texture, Dest, Image->GetSizeF(), PositionChanged);
}

void UIDirect3D9Window::DrawTexture(D3D9Shader *Shader, D3D9Texture *Texture, QRectF *Dest, QSizeF *Size, bool &PositionChanged)
{
    if (!Shader || !Texture || !Dest || !m_d3dDevice || !Size)
        return;

    if (PositionChanged || !Texture->m_verticesUpdated)
    {
        PositionChanged = false;
        UpdateVertices(Texture, Dest, Size);
    }

    SetBlend(true);
    Shader->m_vertexConstants->SetMatrix(m_d3dDevice, "Projection", m_currentProjection);
    Shader->m_vertexConstants->SetMatrix(m_d3dDevice, "Transform", &m_transforms[m_currentTransformIndex].m);
    m_d3dDevice->SetTexture(0, (LPDIRECT3DBASETEXTURE9)Texture->m_texture);
    m_d3dDevice->SetVertexShader(Shader->m_vertexShader);
    m_d3dDevice->SetPixelShader(Shader->m_pixelShader);
    m_d3dDevice->SetStreamSource(0, Texture->m_vertexBuffer, 0, 5 * sizeof(float));
    m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 4);
}

void UIDirect3D9Window::SetRenderTarget(D3D9Texture *Texture)
{
    if (!m_d3dDevice)
        return;

    if (Texture && Texture->m_texture)
    {
        IDirect3DSurface9 *surface = NULL;
        if (SUCCEEDED(Texture->m_texture->GetSurfaceLevel(0, &surface)))
        {
            if (m_currentRenderTarget == surface)
            {
                surface->Release();
                return;
            }

            if (FAILED(m_d3dDevice->SetRenderTarget(0, surface)))
            {
                LOG(VB_GENERAL, LOG_ERR, "Failed to set render target");
                surface->Release();
            }
            else
            {
                m_currentRenderTarget = surface;
            }
        }
    }
    else
    {
        if (m_currentRenderTarget)
        {
            m_currentRenderTarget->Release();
            m_currentRenderTarget = NULL;
        }

        if (m_defaultRenderTarget && FAILED(m_d3dDevice->SetRenderTarget(0, m_defaultRenderTarget)))
            LOG(VB_GENERAL, LOG_ERR, "Failed to set render target");
    }
}

void UIDirect3D9Window::DrawTexture(D3D9Texture *Texture, QRectF *Dest, QSizeF *Size, bool &PositionChanged)
{
    static const float studiorange  = 219.0f / 255.0f;
    static const float studiooffset = 16.0f / 255.0f;

    if (!Texture || !Dest || !m_d3dDevice || !Size)
        return;

    SetBlend(true);

    if (PositionChanged || !Texture->m_verticesUpdated)
    {
        PositionChanged = false;
        UpdateVertices(Texture, Dest, Size);
    }

    // TODO some basic state tracking
    // TODO null pointer checks

    D3DXVECTOR4 color;
    color.x = m_transforms[m_currentTransformIndex].color;
    color.y = m_studioLevels ? studiorange : 1.0f;
    color.z = m_studioLevels ? studiooffset : 0.0f;
    color.w = m_transforms[m_currentTransformIndex].alpha;
    m_defaultShader->m_vertexConstants->SetMatrix(m_d3dDevice, "Projection", m_currentProjection);
    m_defaultShader->m_vertexConstants->SetMatrix(m_d3dDevice, "Transform", &m_transforms[m_currentTransformIndex].m);
    m_defaultShader->m_pixelConstants->SetVector(m_d3dDevice, "Color", &color);
    //uint index = m_defaultShader->m_pixelConstants->GetSamplerIndex("MySampler");
    m_d3dDevice->SetTexture(0, (LPDIRECT3DBASETEXTURE9)Texture->m_texture);
    m_d3dDevice->SetVertexShader(m_defaultShader->m_vertexShader);
    m_d3dDevice->SetPixelShader(m_defaultShader->m_pixelShader);
    m_d3dDevice->SetStreamSource(0, Texture->m_vertexBuffer, 0, 5 * sizeof(float));
    m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 4);
}

void UIDirect3D9Window::DrawShape(UIEffect *Effect, QRectF *Dest, bool &PositionChanged, UIShapePath *Path)
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

bool UIDirect3D9Window::PushEffect(const UIEffect *Effect, const QRectF *Dest)
{
    return PushTransformation(Effect, Dest);
}

void UIDirect3D9Window::PopEffect(void)
{
    PopTransformation();
}

void UIDirect3D9Window::PushClip(const QRect &Rect)
{
    PushClipRect(m_d3dDevice, Rect);
}

void UIDirect3D9Window::PopClip(void)
{
    PopClipRect(m_d3dDevice);
}

void UIDirect3D9Window::SetBlend(bool Enable)
{
    if (Enable == m_blend)
        return;

    if (m_d3dDevice)
    {
        m_blend = Enable;
        m_d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, m_blend);
    }
}

QPaintEngine* UIDirect3D9Window::paintEngine(void) const
{
    return NULL;
}

IDirect3DDevice9* UIDirect3D9Window::GetDevice(void) const
{
    return m_d3dDevice;
}
