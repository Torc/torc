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

#include "winuser.h"

#ifndef WM_APPCOMMAND
#define WM_APPCOMMAND 0x0319
#endif
#define WM_APPCOMMAND_MASK  0xF000
#define WM_APPCOMMAND_MOUSE 0x8000
#define WM_APPCOMMAND_KEY   0
#define WM_APPCOMMAND_OEM   0x1000

#define GET_APPCOMMAND(lParam) ((short)(HIWORD(lParam) & ~WM_APPCOMMAND_MASK))
#define GET_DEVICE(lParam)     ((WORD)(HIWORD(lParam) & WM_APPCOMMAND_MASK))
#define GET_KEYSTATE(lParam)   (LOWORD(lParam))

#define WM_APPCOMMAND_BROWSER_BACKWARD                  1
#define WM_APPCOMMAND_BROWSER_FORWARD                   2
#define WM_APPCOMMAND_BROWSER_REFRESH                   3
#define WM_APPCOMMAND_BROWSER_STOP                      4
#define WM_APPCOMMAND_BROWSER_SEARCH                    5
#define WM_APPCOMMAND_BROWSER_FAVORITES                 6
#define WM_APPCOMMAND_BROWSER_HOME                      7
#define WM_APPCOMMAND_VOLUME_MUTE                       8
#define WM_APPCOMMAND_VOLUME_DOWN                       9
#define WM_APPCOMMAND_VOLUME_UP                         10
#define WM_APPCOMMAND_MEDIA_NEXTTRACK                   11
#define WM_APPCOMMAND_MEDIA_PREVIOUSTRACK               12
#define WM_APPCOMMAND_MEDIA_STOP                        13
#define WM_APPCOMMAND_MEDIA_PLAY_PAUSE                  14
#define WM_APPCOMMAND_LAUNCH_MEDIA_SELECT               16
#define WM_APPCOMMAND_LAUNCH_APP1                       17
#define WM_APPCOMMAND_LAUNCH_APP2                       18
#define WM_APPCOMMAND_BASS_DOWN                         19
#define WM_APPCOMMAND_BASS_BOOST                        20
#define WM_APPCOMMAND_BASS_UP                           21
#define WM_APPCOMMAND_TREBLE_DOWN                       22
#define WM_APPCOMMAND_TREBLE_UP                         23
#define WM_APPCOMMAND_MICROPHONE_VOLUME_MUTE            24
#define WM_APPCOMMAND_MICROPHONE_VOLUME_DOWN            25
#define WM_APPCOMMAND_MICROPHONE_VOLUME_UP              26
#define WM_APPCOMMAND_HELP                              27
#define WM_APPCOMMAND_FIND                              28
#define WM_APPCOMMAND_NEW                               29
#define WM_APPCOMMAND_OPEN                              30
#define WM_APPCOMMAND_CLOSE                             31
#define WM_APPCOMMAND_SAVE                              32
#define WM_APPCOMMAND_PRINT                             33
#define WM_APPCOMMAND_UNDO                              34
#define WM_APPCOMMAND_REDO                              35
#define WM_APPCOMMAND_COPY                              36
#define WM_APPCOMMAND_CUT                               37
#define WM_APPCOMMAND_PASTE                             38
#define WM_APPCOMMAND_REPLY_TO_MAIL                     39
#define WM_APPCOMMAND_FORWARD_MAIL                      40
#define WM_APPCOMMAND_SEND_MAIL                         41
#define WM_APPCOMMAND_SPELL_CHECK                       42
#define WM_APPCOMMAND_DICTATE_OR_COMMAND_CONTROL_TOGGLE 43
#define WM_APPCOMMAND_MIC_ON_OFF_TOGGLE                 44
#define WM_APPCOMMAND_CORRECTION_LIST                   45
#define WM_APPCOMMAND_MEDIA_PLAY                        46
#define WM_APPCOMMAND_MEDIA_PAUSE                       47
#define WM_APPCOMMAND_MEDIA_RECORD                      48
#define WM_APPCOMMAND_MEDIA_FAST_FORWARD                49
#define WM_APPCOMMAND_MEDIA_REWIND                      50
#define WM_APPCOMMAND_MEDIA_CHANNEL_UP                  51
#define WM_APPCOMMAND_MEDIA_CHANNEL_DOWN                52
#define WM_APPCOMMAND_MAX                               52

#define MS_GREENBUTTON     0x0D
#define MS_DVDMENU         0x24
#define MS_LIVETV          0x25
#define MS_ZOOM            0x27
#define MS_EJECT           0x28
#define MS_CLOSEDCAPTION   0x2B
#define MS_RESERVED3       0x2C
#define MS_SUBAUDIO        0x2D
#define MS_EXT0            0x32
#define MS_EXT1            0x33
#define MS_EXT2            0x34
#define MS_EXT3            0x35
#define MS_EXT4            0x36
#define MS_EXT5            0x37
#define MS_EXT6            0x38
#define MS_EXT7            0x39
#define MS_EXT8            0x3A
#define MS_EXTRAS          0x3C
#define MS_EXTRASAPP       0x3D
#define MS_10              0x3E
#define MS_11              0x3F
#define MS_12              0x40
#define MS_RESERVED5       0x41
#define MS_CHANNELINPUT    0x42
#define MS_DVDTOPMENU      0x43
#define MS_MUSIC           0x47
#define MS_RECORDEDTV      0x48
#define MS_PICTURES        0x49
#define MS_VIDEOS          0x4A
#define MS_DVDANGLE        0x4B
#define MS_DVDAUDIO        0x4C
#define MS_DVDSUBTITLE     0x4D
#define MS_RESERVED1       0x4F
#define MS_FMRADIO         0x50
#define MS_TELETEXT        0x5A
#define MS_TELETEXT_RED    0x5B
#define MS_TELETEXT_GREEN  0x5C
#define MS_TELETEXT_YELLOW 0x5D
#define MS_TELETEXT_BLUE   0x5E
#define MS_RESERVED2       0x6A
#define MS_EXT11           0x6F
#define MS_RESERVED4       0x78
#define MS_EXT9            0x80
#define MS_EXT10           0x81

#define AC_PRINT           0x0208
#define AC_PROPERTIES      0x0209
#define AC_PROGRAM_GUIDE   0x008D

typedef LPDIRECT3D9 (WINAPI *LPFND3DC)(UINT SDKVersion);

#define BLACKLIST QString("MainLoop,close,hide,lower,raise,repaint,setDisabled,setEnabled,setFocus,"\
                          "setHidden,setShown,setStyleSheet,setVisible,setWindowModified,setWindowTitle,"\
                          "show,showFullScreen,showMaximized,showMinimized,showNormal,update")

/*! \class UIDirect3D9Window
 *  \brief The main window class on windows (Windows XP SP2 and later)
 *
 * UIDirect3D9Window uses Direct3D9 and HLSL shaders to render the UI and associated
 * media and is feature equivalent with UIOpenGLWindow.
 *
 * Various windows events are handled and UIDirect3D9 also registers and listens for
 * events generated by MCE remotes.
 *
 * Due to the way MCE remotes are implemented however (via multiple
 * HID devices and usage pages) a single remote keypress may generate up to 3 events
 * - a WM_APPCOMMAND event
 * - either: a raw HID event (using then MS vendor specific usage page)
 * - or:     a raw HID event (using the consumer control usage page)
 * - a regular keypress event
 *
 * To avoid multiple actions being generated, WM_APPCOMMAND and raw HID events that are known
 * to generate separate keypress events are filtered out. This may vary by driver and/or device
 * and hence may need additional work.
 *
 * \attention There is no default Qt paint engine implemenation. Rendering via WebKit will fail.
 *
 * \sa UIOpenGLWIndow
*/

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
    // register for raw input
    RAWINPUTDEVICE rid[2];

    // media centre buttons (DVD, Music, Pictures, TV)
    rid[0].usUsagePage = 0xFFBC;
    rid[0].usUsage = 0x88;
    rid[0].dwFlags = RIDEV_INPUTSINK;
    rid[0].hwndTarget = winId();

    // consumer controls (Guide, Details)
    // but may also double up various buttons via WM_INPUT and WM_APPCOMMAND
    rid[1].usUsagePage = 0x0C;
    rid[1].usUsage = 0x01;
    rid[1].dwFlags = RIDEV_INPUTSINK;
    rid[1].hwndTarget = winId();

    if (!RegisterRawInputDevices(rid, 2, sizeof(rid[0])))
       LOG(VB_GENERAL, LOG_ERR, "Failed to register raw input devices.");

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
    // certain events are filtered out by Qt and are never passed on
    // (e.g. WM_QUERYENDSESSION/ENDSESSION)
    // TODO WM_MEDIA_CHANGE

    if (!Message || !Result)
        return false;

    switch (Message->message)
    {
        // see comments in libs/libtorc-core/platforms/torcpowerwin.cpp
        case WM_POWERBROADCAST:
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
            else if (Message->wParam == PBT_APMPOWERSTATUSCHANGE)
            {
                gPower->Refresh();
            }
            break;
        case WM_INPUT:
           return HandleRawInput(Message, Result);
           break;
        case WM_APPCOMMAND:
            return HandleAppCommand(Message, Result);
            break;
        case WM_DEVICECHANGE:
            gLocalContext->NotifyEvent(Torc::USBRescan);
            break;
        default:
            break;
    }

    return false;
}

bool UIDirect3D9Window::HandleRawInput(MSG *Message, long *Result)
{
    // check the raw data size
    UINT size = 0;
    GetRawInputData((HRAWINPUT)Message->lParam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));

    if (size < 2)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to get raw input data");
        return false;
    }

    // retrieve the raw data
    QByteArray buffer(size, 0);
    if (size == GetRawInputData((HRAWINPUT)Message->lParam, RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER)))
    {
        RAWINPUT* rawinput = (RAWINPUT*)buffer.data();
        // HID and valid data size
        if (rawinput->header.dwType == RIM_TYPEHID && rawinput->data.hid.dwCount > 0)
        {
            // retrieve the device info to filter on usage page
            RID_DEVICE_INFO deviceinfo;
            memset(&deviceinfo, 0, sizeof(RID_DEVICE_INFO));
            uint devsize = sizeof(RID_DEVICE_INFO);
            deviceinfo.cbSize = devsize;
            if (GetRawInputDeviceInfo(rawinput->header.hDevice, RIDI_DEVICEINFO, (void*)&deviceinfo, &devsize) > 0 &&
                deviceinfo.dwType == RIM_TYPEHID)
            {
                bool mcebutton = deviceinfo.hid.usUsagePage == 0xFFBC && deviceinfo.hid.usUsage == 0x88;
                bool consumer  = deviceinfo.hid.usUsagePage == 0x000C && deviceinfo.hid.usUsage == 0x01;

                if (mcebutton || consumer)
                {
                    LOG(VB_GUI, LOG_DEBUG, QString("HID device: vendor %1 product %2 version %3 page 0x%4 usage 0x%5")
                        .arg(deviceinfo.hid.dwVendorId).arg(deviceinfo.hid.dwProductId).arg(deviceinfo.hid.dwVersionNumber)
                        .arg(deviceinfo.hid.usUsagePage, 0, 16).arg(deviceinfo.hid.usUsage, 0,16));

                    QByteArray rawdata((const char*)&rawinput->data.hid.bRawData, (int)rawinput->data.hid.dwSizeHid);
                    int button = 0;
                    // first byte contains other info
                    for (int i = 1; i < rawdata.size(); ++i)
                        button += ((quint8)rawdata[i]) << ((i -1) * 8);

                    if (button && consumer && (button != AC_PROGRAM_GUIDE && button != AC_PROPERTIES))
                    {
                        LOG(VB_GENERAL, LOG_INFO, QString("Ignoring consumer control button 0x%1")
                            .arg(button, 0, 16));
                        button = 0;
                    }

                    int key = 0;
                    QString context;
                    if (button && mcebutton)
                    {
                        context = "HID Vendor Control";

                        switch (button)
                        {
                            case MS_GREENBUTTON:    key = Qt::Key_OfficeHome; break;
                            case MS_DVDMENU:        key = Qt::Key_Menu; break;
                            case MS_LIVETV:         key = Qt::Key_Messenger; break;
                            case MS_ZOOM:           key = Qt::Key_Zoom; break;
                            case MS_EJECT:          key = Qt::Key_Eject; break;
                            case MS_CLOSEDCAPTION:  key = Qt::Key_Subtitle; break;
                            case MS_RESERVED3:      key = Qt::Key_F27; break;
                            case MS_SUBAUDIO:       key = Qt::Key_AudioCycleTrack; break;
                            case MS_EXT0:           key = Qt::Key_Launch5; break;
                            case MS_EXT1:           key = Qt::Key_Launch6; break;
                            case MS_EXT2:           key = Qt::Key_Launch7; break;
                            case MS_EXT3:           key = Qt::Key_Launch8; break;
                            case MS_EXT4:           key = Qt::Key_Launch9; break;
                            case MS_EXT5:           key = Qt::Key_LaunchA; break;
                            case MS_EXT6:           key = Qt::Key_LaunchB; break;
                            case MS_EXT7:           key = Qt::Key_LaunchC; break;
                            case MS_EXT8:           key = Qt::Key_LaunchD; break;
                            case MS_EXTRAS:         key = Qt::Key_AudioForward; break;
                            case MS_EXTRASAPP:      key = Qt::Key_AudioRepeat; break;
                            case MS_10:             key = Qt::Key_Flip; break;
                            case MS_11:             key = Qt::Key_Hangup; break;
                            case MS_12:             key = Qt::Key_VoiceDial; break;
                            case MS_RESERVED5:      key = Qt::Key_F29; break;
                            case MS_CHANNELINPUT:   key = Qt::Key_C; break; // see TorcCECDevice for consistency
                            case MS_DVDTOPMENU:     key = Qt::Key_TopMenu; break;
                            case MS_MUSIC:          key = Qt::Key_Music; break;
                            case MS_RECORDEDTV:     key = Qt::Key_Execute; break;
                            case MS_PICTURES:       key = Qt::Key_Pictures; break;
                            case MS_VIDEOS:         key = Qt::Key_Video; break;
                            case MS_DVDANGLE:       key = Qt::Key_F24; break; // see TorcCECDevice for consistency
                            case MS_DVDAUDIO:       key = Qt::Key_Plus; break; // see TorcCECDevice for consistency
                            case MS_DVDSUBTITLE:    key = Qt::Key_Time; break;
                            case MS_RESERVED1:      key = Qt::Key_F25; break;
                            case MS_FMRADIO:        key = Qt::Key_LastNumberRedial; break;
                            case MS_TELETEXT:       key = Qt::Key_F7; break;
                            case MS_TELETEXT_RED:   key = Qt::Key_F2; break; // see TorcCECDevice for consistency
                            case MS_TELETEXT_GREEN: key = Qt::Key_F3; break;
                            case MS_TELETEXT_YELLOW: key = Qt::Key_F4; break;
                            case MS_TELETEXT_BLUE:  key = Qt::Key_F5; break;
                            case MS_RESERVED2:      key = Qt::Key_F26; break;
                            case MS_EXT11:          key = Qt::Key_LaunchG; break;
                            case MS_RESERVED4:      key = Qt::Key_F28; break;
                            case MS_EXT9:           key = Qt::Key_LaunchE; break;
                            case MS_EXT10:          key = Qt::Key_LaunchF; break;
                        }
                    }
                    else if (button && consumer)
                    {
                        context = "HID Consumer Control";

                        switch (button)
                        {
                            case AC_PROPERTIES:     key = Qt::Key_I; break;
                            case AC_PROGRAM_GUIDE:  key = Qt::Key_S; break;
                        }
                    }

                    if (key)
                    {
                        QKeyEvent *keyevent = new QKeyEvent(QEvent::KeyPress, key, TORC_KEYEVENT_MODIFIERS, context);

                        if (gLocalContext->GetUIObject())
                            QApplication::postEvent(gLocalContext->GetUIObject(), keyevent);

                        *Result = 0;
                        LOG(VB_GENERAL, LOG_INFO, QString("WM_INPUT: button 0x%1").arg(button, 0, 16));
                        return true;
                    }
                }
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, "Failed to get device info");
            }
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to retrieve raw input data");
    }

    return false;
}

bool UIDirect3D9Window::HandleAppCommand(MSG *Message, long *Result)
{
    if (!Message || !Result)
        return false;

    static const QString keycontext    = "HID Keyboard";
    static const QString mousecontext  = "HID Mouse";
    static const QString remotecontext = "HID Remote";

    short command = GET_APPCOMMAND(Message->lParam);
    WORD device   = GET_DEVICE(Message->lParam);

    QString context = device == WM_APPCOMMAND_KEY ? keycontext :
                      device == WM_APPCOMMAND_OEM ? remotecontext :
                      device == WM_APPCOMMAND_MOUSE ? mousecontext : QString();

    if (command < 0 || command > WM_APPCOMMAND_MAX || context.isEmpty())
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("Unrecognised APPCOMMAND %1 (device %2)")
            .arg(command).arg(context));
        return false;
    }

    int key = 0;
    switch (command)
    {
        // filter out commands that will also trigger a regular keypress
        case WM_APPCOMMAND_VOLUME_MUTE:
        case WM_APPCOMMAND_VOLUME_DOWN:
        case WM_APPCOMMAND_VOLUME_UP:
            LOG(VB_GENERAL, LOG_INFO, QString("Ignoring WM_APPCOMMAND: %1 device: %2").arg(command).arg(context));
            break;
        // and allow the rest
        case WM_APPCOMMAND_BROWSER_BACKWARD:                    key = Qt::Key_Back; break;
        case WM_APPCOMMAND_BROWSER_FORWARD:                     key = Qt::Key_Forward; break;
        case WM_APPCOMMAND_BROWSER_REFRESH:                     key = Qt::Key_Refresh; break;
        case WM_APPCOMMAND_BROWSER_STOP:                        key = Qt::Key_Stop; break;
        case WM_APPCOMMAND_BROWSER_SEARCH:                      key = Qt::Key_Search; break;
        case WM_APPCOMMAND_BROWSER_FAVORITES:                   key = Qt::Key_Favorites; break;
        case WM_APPCOMMAND_BROWSER_HOME:                        key = Qt::Key_HomePage; break;
        case WM_APPCOMMAND_MEDIA_NEXTTRACK:                     key = Qt::Key_MediaNext; break;
        case WM_APPCOMMAND_MEDIA_PREVIOUSTRACK:                 key = Qt::Key_MediaPrevious; break;
        case WM_APPCOMMAND_MEDIA_STOP:                          key = Qt::Key_MediaStop; break;
        case WM_APPCOMMAND_MEDIA_PLAY_PAUSE:                    key = Qt::Key_MediaTogglePlayPause; break;
        case WM_APPCOMMAND_LAUNCH_MEDIA_SELECT:                 key = Qt::Key_LaunchMedia; break;
        case WM_APPCOMMAND_LAUNCH_APP1:                         key = Qt::Key_Launch0; break;
        case WM_APPCOMMAND_LAUNCH_APP2:                         key = Qt::Key_Launch1; break;
        case WM_APPCOMMAND_BASS_DOWN:                           key = Qt::Key_BassDown; break;
        case WM_APPCOMMAND_BASS_BOOST:                          key = Qt::Key_BassBoost; break;
        case WM_APPCOMMAND_BASS_UP:                             key = Qt::Key_BassUp; break;
        case WM_APPCOMMAND_TREBLE_DOWN:                         key = Qt::Key_TrebleDown; break;
        case WM_APPCOMMAND_TREBLE_UP:                           key = Qt::Key_TrebleUp; break;
        case WM_APPCOMMAND_MICROPHONE_VOLUME_MUTE:              key = Qt::Key_Launch2; break;
        case WM_APPCOMMAND_MICROPHONE_VOLUME_DOWN:              key = Qt::Key_Launch3; break;
        case WM_APPCOMMAND_MICROPHONE_VOLUME_UP:                key = Qt::Key_Launch4; break;
        case WM_APPCOMMAND_HELP:                                key = Qt::Key_Help; break;
        case WM_APPCOMMAND_FIND:                                key = Qt::Key_F; break;
        case WM_APPCOMMAND_NEW:                                 key = Qt::Key_N; break;
        case WM_APPCOMMAND_OPEN:                                key = Qt::Key_O; break;
        case WM_APPCOMMAND_CLOSE:                               key = Qt::Key_Close; break;
        case WM_APPCOMMAND_SAVE:                                key = Qt::Key_Save; break;
        case WM_APPCOMMAND_PRINT:                               key = Qt::Key_Print; break;
        case WM_APPCOMMAND_UNDO:                                key = Qt::Key_Context1; break;
        case WM_APPCOMMAND_REDO:                                key = Qt::Key_Context2; break;
        case WM_APPCOMMAND_COPY:                                key = Qt::Key_Copy; break;
        case WM_APPCOMMAND_CUT:                                 key = Qt::Key_Cut; break;
        case WM_APPCOMMAND_PASTE:                               key = Qt::Key_Paste; break;
        case WM_APPCOMMAND_REPLY_TO_MAIL:                       key = Qt::Key_Reply; break;
        case WM_APPCOMMAND_FORWARD_MAIL:                        key = Qt::Key_MailForward; break;
        case WM_APPCOMMAND_SEND_MAIL:                           key = Qt::Key_Send; break;
        case WM_APPCOMMAND_SPELL_CHECK:                         key = Qt::Key_Spell; break;
        case WM_APPCOMMAND_DICTATE_OR_COMMAND_CONTROL_TOGGLE:   key = Qt::Key_Context3; break;
        case WM_APPCOMMAND_MIC_ON_OFF_TOGGLE:                   key = Qt::Key_WebCam; break;
        case WM_APPCOMMAND_CORRECTION_LIST:                     key = Qt::Key_ToDoList; break;
        case WM_APPCOMMAND_MEDIA_PLAY:                          key = Qt::Key_MediaPlay; break;
        case WM_APPCOMMAND_MEDIA_PAUSE:                         key = Qt::Key_MediaPause; break;
        case WM_APPCOMMAND_MEDIA_RECORD:                        key = Qt::Key_MediaRecord; break;
        case WM_APPCOMMAND_MEDIA_FAST_FORWARD:                  key = Qt::Key_F22; break; // see TorcCECDevice for consistency
        case WM_APPCOMMAND_MEDIA_REWIND:                        key = Qt::Key_F23; break;
        case WM_APPCOMMAND_MEDIA_CHANNEL_UP:                    key = Qt::Key_F21; break;
        case WM_APPCOMMAND_MEDIA_CHANNEL_DOWN:                  key = Qt::Key_F20; break;
    }

    if (key)
    {
        QKeyEvent *keyevent = new QKeyEvent(QEvent::KeyPress, key, TORC_KEYEVENT_MODIFIERS, context);

        if (gLocalContext->GetUIObject())
            QApplication::postEvent(gLocalContext->GetUIObject(), keyevent);

        LOG(VB_GENERAL, LOG_DEBUG, QString("APPCOMMAND: %1 device: %2").arg(command).arg(context));
        *Result = TRUE;
        return true;
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
