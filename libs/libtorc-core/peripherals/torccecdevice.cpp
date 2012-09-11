/* Class TorcCECDevice/TorcCECDeviceHandler
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

// Torc
#include "torclocalcontext.h"
#include "torclogging.h"
#include "torcevent.h"
#include "torcthread.h"
#include "torcadminthread.h"
#include "torcusb.h"
#include "torcedid.h"
#include "torccecdevice.h"

// libCEC
#include <libcec/cec.h>
#include <iostream>
using namespace CEC;
using namespace std;
#include <libcec/cecloader.h>

#define USING_LIBCEC_VERSION CEC_CLIENT_VERSION_1_7_2
#define MAX_CEC_DEVICES 10
#define OSDNAME "Torc"

TorcCECDevice *gCECDevice = NULL;
QMutex    *gCECDeviceLock = new QMutex(QMutex::Recursive);

/*! \class TorcCECDevicePriv
 *  \brief A class that interfaces with libCEC and supported devices.
 *
 * TorcCECDevicePriv is a singleton class that will attempt to open the first
 * detected device. libCEC currently operates using a single static instance of
 * the library; hence multiple devices will not work. Although such a setup is
 * unlikely, TorcCECDeviceHandler protects aginst this.
 *
 * \todo Add setting for deviceTypes (e.g CEC_DEVICE_TYPE_RECORDING_DEVICE)
 * \todo Add settings for device types to wake and poweroff (e.g. CECDEVICE_AUDIOSYSTEM)
 * \todo Set device language
 *
 * \sa TorcEDID
*/

class TorcCECDevicePriv
{
  public:
    TorcCECDevicePriv()
      : m_adapter(NULL),
        m_defaultDevice(LIBCEC_DEVICE_DEFAULT),
        m_physicalAddressAutoDetected(false),
        m_defaultPhysicalAddress(LIBCEC_PHYSICALADDRESS_DEFAULT),
        m_defaultHDMIPort(LIBCEC_HDMIPORT_DEFAULT),
        m_defaultBaseDevice(LIBCEC_BASEDEVICE_DEFAULT),
        m_powerOffTV(false),
        m_powerOffTVOnExit(LIBCEC_POWEROFFTV_ONEXIT_DEFAULT),
        m_powerOnTV(false),
        m_powerOnTVOnStart(LIBCEC_POWERONTV_ONSTART_DEFAULT),
        m_makeActiveSource(false),
        m_makeActiveSourceOnStart(LIBCEC_MAKEACTIVESOURCE_DEFAULT),
        m_sendInactiveSource(false),
        m_sendInactiveSourceDefault(LIBCEC_SENDINACTIVESOURCE_DEFAULT)
    {
        m_callbacks.CBCecLogMessage           = &LogMessageCallback;
        m_callbacks.CBCecKeyPress             = &KeyPressCallback;
        m_callbacks.CBCecConfigurationChanged = &ConfigCallback;
        m_callbacks.CBCecCommand              = &CommandCallback;
        m_callbacks.CBCecAlert                = &AlertCallback;
        m_callbacks.CBCecSourceActivated      = &SourceActivatedCallback;
        m_callbacks.CBCecMenuStateChanged     = &MenuStateCallback;
    }

    ~TorcCECDevicePriv()
    {
        Close(false, false);
    }

    bool Open(void)
    {
        if (m_adapter)
            return true;

        LOG(VB_GENERAL, LOG_INFO, QString("Creating libCEC device (compiled with version %1)")
            .arg(LIBCEC_VERSION_CURRENT, 0, 16));

        qint16 detectedphysicaladdress = TorcEDID::PhysicalAddress();

        // create adapter interface
        libcec_configuration config;
        config.Clear();
        config.clientVersion = USING_LIBCEC_VERSION;
        int length = sizeof(OSDNAME);
        if (length > 13)
            length = 13;
        memcpy(config.strDeviceName, OSDNAME, length);
        config.deviceTypes.Add(CEC_DEVICE_TYPE_PLAYBACK_DEVICE);
        //config.bAutodetectAddress
        config.iPhysicalAddress = detectedphysicaladdress ? detectedphysicaladdress : m_defaultPhysicalAddress;
        config.baseDevice       = (cec_logical_address)m_defaultBaseDevice;
        config.iHDMIPort        = m_defaultHDMIPort;
        //config.tvVendor
        config.wakeDevices.Set(m_powerOnTVOnStart ? CECDEVICE_TV : CECDEVICE_UNKNOWN);
        config.powerOffDevices.Set(m_powerOffTVOnExit ? CECDEVICE_TV : CECDEVICE_UNKNOWN);
        //config.serverVersion

        //config.bGetSettingsFromROM
        //config.bUseTVMenuLanguage
        config.bActivateSource    = (int)m_makeActiveSource;
        //config.bPowerOffScreensaver
        //config.bPowerOffOnStandby
        config.bSendInactiveSource = (int)m_sendInactiveSource;

        config.callbackParam       = (void*)this;
        config.callbacks           = &m_callbacks;

        //config.logicalAddresses
        //config.iFirmwareVersion
        //config.bPowerOffDevicesOnStandby
        //config.bShutdownOnStandby
        //config.strDeviceLanguage
        //config.iFirmwareBuildDate
        //config.bMonitorOnly
        //config.cecVersion

        // open library
        m_adapter = LibCecInitialise(&config);

        if (!m_adapter)
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to load libcec.");
            return false;
        }

        // version check
        if (config.serverVersion < USING_LIBCEC_VERSION)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("The installed libCEC supports version %1. "
                        "This version of Torc needs at least version %2.")
                .arg(config.serverVersion, 0, 16)
                .arg(USING_LIBCEC_VERSION, 0, 16));
            m_adapter->EnableCallbacks(NULL, NULL);
            UnloadLibCec(m_adapter);
            m_adapter = NULL;
            return false;
        }

        // find adapters
        cec_adapter *devices = new cec_adapter[MAX_CEC_DEVICES];
        uint8_t devicecount  = m_adapter->FindAdapters(devices, MAX_CEC_DEVICES, NULL);
        if (devicecount < 1)
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to find any CEC devices.");
            m_adapter->EnableCallbacks(NULL, NULL);
            UnloadLibCec(m_adapter);
            m_adapter = NULL;
            delete [] devices;
            return false;
        }

        // find required device and debug
        int devicenum = 0;
        bool find = m_defaultDevice != "auto";

        if (devicecount > 1)
            LOG(VB_GENERAL, LOG_INFO, QString("Found %1 CEC devices(s)").arg(devicecount));

        for (uint8_t i = 0; i < devicecount; i++)
        {
            QString comm = QString::fromAscii(devices[i].comm);
            bool match = find ? (comm == m_defaultDevice) : (i == 0);
            devicenum = match ? i : devicenum;
            LOG(VB_GENERAL, LOG_INFO,
                QString("Device %1: path '%2' com port '%3' %4").arg(i + 1)
                .arg(QString::fromAscii(devices[i].path)).arg(comm)
                .arg(match ? "SELECTED" : ""));
        }

        // open adapter
        QString path = QString::fromAscii(devices[devicenum].path);
        QString comm = QString::fromAscii(devices[devicenum].comm);
        LOG(VB_GENERAL, LOG_INFO, QString("Trying to open device %1 (%2)").arg(path).arg(comm));

        m_adapter->InitVideoStandalone();

        if (!m_adapter->Open(devices[devicenum].comm))
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to open device.");
            m_adapter->EnableCallbacks(NULL, NULL);
            UnloadLibCec(m_adapter);
            m_adapter = NULL;
            delete [] devices;
            return false;
        }

        libcec_configuration newconfig;
        m_adapter->GetCurrentConfiguration(&newconfig);
        m_physicalAddressAutoDetected = newconfig.bAutodetectAddress;

        if (m_physicalAddressAutoDetected)
        {
            LOG(VB_GENERAL, LOG_INFO, "HDMI physical address was autodected by libCEC");
        }
        else if (detectedphysicaladdress)
        {
            LOG(VB_GENERAL, LOG_INFO, "HDMI physical address was autodected by Torc");
            m_physicalAddressAutoDetected = true;
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, "HDMI physical address was NOT autodected. "
                                      "If you experience issues, ensure your settings are correct.");
        }

        // setup initial actions
        m_powerOnTV        = m_powerOnTVOnStart;
        m_makeActiveSource = m_makeActiveSourceOnStart;
        ProcessActions();

        delete [] devices;
        return true;
    }

    void Close(bool Reinitialising, bool Suspending)
    {
        if (!Reinitialising)
        {
            // make inactive (if configured)
            m_sendInactiveSource = m_sendInactiveSourceDefault;

            // turn off tv (if configured)
            m_powerOffTV = m_powerOffTVOnExit;
            ProcessActions();
        }

        if (m_adapter)
        {
            LOG(VB_GENERAL, LOG_INFO, "Destroying libCEC device");
            m_adapter->EnableCallbacks(NULL, NULL);
            m_adapter->Close();
            UnloadLibCec(m_adapter);
            m_adapter = NULL;
        }
    }

    void ProcessActions(void)
    {
        if (!m_adapter)
            return;

        // power state
        if (m_powerOffTV)
        {
            if (m_adapter->StandbyDevices())
                LOG(VB_GENERAL, LOG_INFO, "Asked TV to turn off");
            else
                LOG(VB_GENERAL, LOG_ERR,  "Failed to turn TV off");
        }

        if (m_powerOnTV)
        {
            if (m_adapter->PowerOnDevices())
                LOG(VB_GENERAL, LOG_INFO, "Asked TV to turn on");
            else
                LOG(VB_GENERAL, LOG_ERR,  "Failed to turn TV on");
        }

        if (m_makeActiveSource)
        {
            if (m_adapter->SetActiveSource())
                LOG(VB_GENERAL, LOG_INFO, "Made active source");
            else
                LOG(VB_GENERAL, LOG_ERR,  "Failed to make source active");
        }

        if (m_sendInactiveSource)
        {
            if (m_adapter->SetInactiveView())
                LOG(VB_GENERAL, LOG_INFO, "Sent inactive view");
            else
                LOG(VB_GENERAL, LOG_ERR,  "Failed to send inactive view");
        }

        m_powerOffTV         = false;
        m_powerOnTV          = false;
        m_makeActiveSource   = false;
        m_sendInactiveSource = false;
    }

    static int CEC_CDECL LogMessageCallback(void *Object, const cec_log_message &Message)
    {
        QString message(Message.message);
        int level = LOG_INFO;

        if (Message.level & CEC_LOG_ERROR)
            level = LOG_ERR;
        else if (Message.level & CEC_LOG_WARNING)
            level = LOG_WARNING;
        else if (Message.level & CEC_LOG_DEBUG || Message.level & CEC_LOG_TRAFFIC)
            level = LOG_DEBUG;

        LOG(VB_GENERAL, level, QString("libCEC: %1").arg(message));
        return 1;
    }

    static int CEC_CDECL KeyPressCallback(void *Object, const cec_keypress &Key)
    {
        QString code;
        int action = 0;
        switch (Key.keycode)
        {
            case CEC_USER_CONTROL_CODE_NUMBER0:
                action = Qt::Key_0;
                code   = "0";
                break;
            case CEC_USER_CONTROL_CODE_NUMBER1:
                action = Qt::Key_1;
                code   = "1";
                break;
            case CEC_USER_CONTROL_CODE_NUMBER2:
                action = Qt::Key_2;
                code   = "2";
                break;
            case CEC_USER_CONTROL_CODE_NUMBER3:
                action = Qt::Key_3;
                code   = "3";
                break;
            case CEC_USER_CONTROL_CODE_NUMBER4:
                action = Qt::Key_4;
                code   = "4";
                break;
            case CEC_USER_CONTROL_CODE_NUMBER5:
                action = Qt::Key_5;
                code   = "5";
                break;
            case CEC_USER_CONTROL_CODE_NUMBER6:
                action = Qt::Key_6;
                code   = "6";
                break;
            case CEC_USER_CONTROL_CODE_NUMBER7:
                action = Qt::Key_7;
                code   = "7";
                break;
            case CEC_USER_CONTROL_CODE_NUMBER8:
                action = Qt::Key_8;
                code   = "8";
                break;
            case CEC_USER_CONTROL_CODE_NUMBER9:
                action = Qt::Key_9;
                code   = "9";
                break;
            case CEC_USER_CONTROL_CODE_SELECT:
                action = Qt::Key_Select;
                code   = "SELECT";
                break;
            case CEC_USER_CONTROL_CODE_ENTER:
                action = Qt::Key_Enter;
                code   = "ENTER";
                break;
            case CEC_USER_CONTROL_CODE_UP:
                action = Qt::Key_Up;
                code   = "UP";
                break;
            case CEC_USER_CONTROL_CODE_DOWN:
                action = Qt::Key_Down;
                code   = "DOWN";
                break;
            case CEC_USER_CONTROL_CODE_LEFT:
                action = Qt::Key_Left;
                code   = "LEFT";
                break;
            case CEC_USER_CONTROL_CODE_LEFT_UP:
                action = Qt::Key_Left;
                code   = "LEFT_UP";
                break;
            case CEC_USER_CONTROL_CODE_LEFT_DOWN:
                action = Qt::Key_Left;
                code   = "LEFT_DOWN";
                break;
            case CEC_USER_CONTROL_CODE_RIGHT:
                action = Qt::Key_Right;
                code   = "RIGHT";
                break;
            case CEC_USER_CONTROL_CODE_RIGHT_UP:
                action = Qt::Key_Right;
                code   = "RIGHT_UP";
                break;
            case CEC_USER_CONTROL_CODE_RIGHT_DOWN:
                action = Qt::Key_Right;
                code   = "RIGHT_DOWN";
                break;
            case CEC_USER_CONTROL_CODE_ROOT_MENU:
                action = Qt::Key_M;
                code   = "ROOT_MENU";
                break;
            case CEC_USER_CONTROL_CODE_EXIT:
                action = Qt::Key_Escape;
                code   = "EXIT";
                break;
            case CEC_USER_CONTROL_CODE_PREVIOUS_CHANNEL:
                action = Qt::Key_H;
                code   = "PREVIOUS_CHANNEL";
                break;
            case CEC_USER_CONTROL_CODE_SOUND_SELECT:
                action = Qt::Key_Plus;
                code   = "SOUND_SELECT";
                break;
            case CEC_USER_CONTROL_CODE_VOLUME_UP:
                action = Qt::Key_VolumeUp;
                code   = "VOLUME_UP";
                break;
            case CEC_USER_CONTROL_CODE_VOLUME_DOWN:
                action = Qt::Key_VolumeDown;
                code   = "VOLUME_DOWN";
                break;
            case CEC_USER_CONTROL_CODE_MUTE:
                action = Qt::Key_VolumeMute;
                code   = "MUTE";
                break;
            case CEC_USER_CONTROL_CODE_PLAY:
                action = Qt::Key_P;
                code   = "PLAY";
                break;
            case CEC_USER_CONTROL_CODE_PAUSE:
                action = Qt::Key_P; // same as play
                code   = "PAUSE";
                break;
            case CEC_USER_CONTROL_CODE_STOP:
                action = Qt::Key_Stop;
                code   = "STOP";
                break;
            case CEC_USER_CONTROL_CODE_RECORD:
                action = Qt::Key_R;
                code   = "RECORD";
                break;
            case CEC_USER_CONTROL_CODE_CLEAR:
                action = Qt::Key_Clear;
                code   = "CLEAR";
                break;
            case CEC_USER_CONTROL_CODE_DISPLAY_INFORMATION:
                action = Qt::Key_I;
                code   = "DISPLAY_INFORMATION";
                break;
            case CEC_USER_CONTROL_CODE_PAGE_UP:
                action = Qt::Key_PageUp;
                code   = "PAGE_UP";
                break;
            case CEC_USER_CONTROL_CODE_PAGE_DOWN:
                action = Qt::Key_PageDown;
                code   = "PAGE_DOWN";
                break;
            case CEC_USER_CONTROL_CODE_EJECT:
                action = Qt::Key_Eject;
                code   = "EJECT";
                break;
            case CEC_USER_CONTROL_CODE_FORWARD:
                action = Qt::Key_Forward;
                code   = "FORWARD";
                break;
            case CEC_USER_CONTROL_CODE_BACKWARD:
                action = Qt::Key_Back;
                code   = "BACKWARD";
                break;
            case CEC_USER_CONTROL_CODE_F1_BLUE:
                action = Qt::Key_F5; // NB F1 is help and we normally map blue to F5
                code   = "F1_BLUE";
                break;
            case CEC_USER_CONTROL_CODE_F2_RED:
                action = Qt::Key_F2;
                code   = "F2_RED";
                break;
            case CEC_USER_CONTROL_CODE_F3_GREEN:
                action = Qt::Key_F3;
                code   = "F3_GREEN";
                break;
            case CEC_USER_CONTROL_CODE_F4_YELLOW:
                action = Qt::Key_F4;
                code   = "F4_YELLOW";
                break;
            case CEC_USER_CONTROL_CODE_SETUP_MENU:
                action = Qt::Key_M; // Duplicate of Root Menu
                code   = "SETUP_MENU";
                break;
            case CEC_USER_CONTROL_CODE_CONTENTS_MENU:
                action = Qt::Key_M; // Duplicate of Root Menu
                code   = "CONTENTS_MENU";
                break;
            case CEC_USER_CONTROL_CODE_FAVORITE_MENU:
                action = Qt::Key_M; // Duplicate of Root Menu
                code   = "FAVORITE_MENU";
                break;
            case CEC_USER_CONTROL_CODE_DOT:
                action = Qt::Key_Period;
                code  = "DOT";
                break;
            case CEC_USER_CONTROL_CODE_NEXT_FAVORITE:
                action = Qt::Key_Slash;
                code   = "NEXT_FAVORITE";
                break;
            case CEC_USER_CONTROL_CODE_INPUT_SELECT:
                action = Qt::Key_C;
                code = "INPUT_SELECT";
                break;
            case CEC_USER_CONTROL_CODE_HELP:
                action = Qt::Key_F1;
                code   = "HELP";
                break;
            case CEC_USER_CONTROL_CODE_STOP_RECORD:
                action = Qt::Key_R; // Duplicate of Record
                code = "STOP_RECORD";
                break;
            case CEC_USER_CONTROL_CODE_SUB_PICTURE:
                action = Qt::Key_V;
                code   = "SUB_PICTURE";
                break;
            case CEC_USER_CONTROL_CODE_ELECTRONIC_PROGRAM_GUIDE:
                action = Qt::Key_S;
                code   = "ELECTRONIC_PROGRAM_GUIDE";
                break;
            case CEC_USER_CONTROL_CODE_POWER:
                action = Qt::Key_PowerOff;
                code = "POWER";
                break;

             // these codes have 'non-standard' Qt key mappings to ensure
             // each code has a unique key mapping
            case CEC_USER_CONTROL_CODE_CHANNEL_DOWN:
                action = Qt::Key_F20; // to differentiate from Up
                code   = "CHANNEL_DOWN";
                break;
            case CEC_USER_CONTROL_CODE_CHANNEL_UP:
                action = Qt::Key_F21; // to differentiate from Down
                code   = "CHANNEL_UP";
                break;
            case CEC_USER_CONTROL_CODE_REWIND:
                action = Qt::Key_F22; // to differentiate from Left
                code   = "REWIND";
                break;
            case CEC_USER_CONTROL_CODE_FAST_FORWARD:
                action = Qt::Key_F23; // to differentiate from Right
                code   = "FAST_FORWARD";
                break;
            case CEC_USER_CONTROL_CODE_ANGLE:
                action = Qt::Key_F24;
                code = "ANGLE";
                break;
            case CEC_USER_CONTROL_CODE_F5:
                action = Qt::Key_F6; // NB!
                code = "F5";
                break;

            // codes with no obvious MythTV action
            case CEC_USER_CONTROL_CODE_INITIAL_CONFIGURATION:
                code = "INITIAL_CONFIGURATION";
                break;
            case CEC_USER_CONTROL_CODE_PAUSE_RECORD:
                code = "PAUSE_RECORD";
                break;
            case CEC_USER_CONTROL_CODE_VIDEO_ON_DEMAND:
                code = "VIDEO_ON_DEMAND";
                break;
            case CEC_USER_CONTROL_CODE_TIMER_PROGRAMMING:
                code = "TIMER_PROGRAMMING";
                break;
            case CEC_USER_CONTROL_CODE_UNKNOWN:
                code = "UNKOWN";
                break;
            case CEC_USER_CONTROL_CODE_DATA:
                code = "DATA";
                break;

            // Functions aren't implemented (similar to macros?)
            case CEC_USER_CONTROL_CODE_POWER_ON_FUNCTION:
                code = "POWER_ON_FUNCTION";
                break;
            case CEC_USER_CONTROL_CODE_PLAY_FUNCTION:
                code = "PLAY_FUNCTION";
                break;
            case CEC_USER_CONTROL_CODE_PAUSE_PLAY_FUNCTION:
                code = "PAUSE_PLAY_FUNCTION";
                break;
            case CEC_USER_CONTROL_CODE_RECORD_FUNCTION:
                code = "RECORD_FUNCTION";
                break;
            case CEC_USER_CONTROL_CODE_PAUSE_RECORD_FUNCTION:
                code = "PAUSE_RECORD_FUNCTION";
                break;
            case CEC_USER_CONTROL_CODE_STOP_FUNCTION:
                code = "STOP_FUNCTION";
                break;
            case CEC_USER_CONTROL_CODE_MUTE_FUNCTION:
                code = "MUTE_FUNCTION";
                break;
            case CEC_USER_CONTROL_CODE_RESTORE_VOLUME_FUNCTION:
                code = "RESTORE_VOLUME_FUNCTION";
                break;
            case CEC_USER_CONTROL_CODE_TUNE_FUNCTION:
                code = "TUNE_FUNCTION";
                break;
            case CEC_USER_CONTROL_CODE_SELECT_MEDIA_FUNCTION:
                code = "SELECT_MEDIA_FUNCTION";
                break;
            case CEC_USER_CONTROL_CODE_SELECT_AV_INPUT_FUNCTION:
                code = "SELECT_AV_INPUT_FUNCTION";
                break;
            case CEC_USER_CONTROL_CODE_SELECT_AUDIO_INPUT_FUNCTION:
                code = "SELECT_AUDIO_INPUT_FUNCTION";
                break;
            case CEC_USER_CONTROL_CODE_POWER_TOGGLE_FUNCTION:
                code = "POWER_TOGGLE_FUNCTION";
                break;
            case CEC_USER_CONTROL_CODE_POWER_OFF_FUNCTION:
                code = "POWER_OFF_FUNCTION";
                break;
        }

        LOG(VB_GENERAL, LOG_INFO, QString("Keypress %1 %2")
            .arg(code).arg(0 == action ? "(Not actioned)" : ""));

        if (0 == action)
            return 1;

        QVariantMap data;
        data.insert("key", action);
        data.insert("source", CEC_KEYPRESS_CONTEXT);
        TorcEvent event(Key.duration > 0 ? Torc::KeyRelease : Torc::KeyPress, data);

        gLocalContext->Notify(event);
        return 1;
    }

    static int CEC_CDECL CommandCallback(void *Object, const cec_command &Command)
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("Command opcode 0x%1").arg(Command.opcode,0,16));

        switch (Command.opcode)
        {
            // TODO
            default:
                break;
        }

        return 1;
    }

    static int CEC_CDECL ConfigCallback(void *Object, const libcec_configuration &Config)
    {
        LOG(VB_GENERAL, LOG_INFO, "Adapter configuration changed.");
        return 1;
    }

    static int CEC_CDECL AlertCallback(void *Object, const libcec_alert Alert, const libcec_parameter &CECParam)
    {
        switch (Alert)
        {
            case CEC_ALERT_CONNECTION_LOST:
            case CEC_ALERT_PERMISSION_ERROR:
            case CEC_ALERT_PORT_BUSY:
                LOG(VB_GENERAL, LOG_WARNING, (const char*)CECParam.paramData);
                break;
            case CEC_ALERT_SERVICE_DEVICE: // already logged
                break;
            default:
                break;
        }

        return 1;
    }

    static int CEC_CDECL MenuStateCallback(void *Object, const cec_menu_state State)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("CEC menu state %1")
            .arg(State == CEC_MENU_STATE_ACTIVATED ? "Activated" : "Deactivated"));
        return 1;
    }

    static void CEC_CDECL SourceActivatedCallback(void *Object, const cec_logical_address Address, const uint8_t Activated)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Source %1 %2")
            .arg(Address).arg(Activated ? "Activated" : "Deactivated"));
    }

  private:
    ICECAdapter  *m_adapter;
    ICECCallbacks m_callbacks;
    QString       m_defaultDevice;
    bool          m_physicalAddressAutoDetected;

    quint16       m_defaultPhysicalAddress;
    quint8        m_defaultHDMIPort;
    quint8        m_defaultBaseDevice;

    bool          m_powerOffTV;
    bool          m_powerOffTVOnExit;
    bool          m_powerOnTV;
    bool          m_powerOnTVOnStart;
    bool          m_makeActiveSource;
    bool          m_makeActiveSourceOnStart;
    bool          m_sendInactiveSource;
    bool          m_sendInactiveSourceDefault;
};

/*! \class TorcCECDevice
  * \brief A singleton class that interacts with the libCEC instance.
  *
  * TorcCECDevice creates and manages a single instance of TorcCECDevicePriv. It
  * listens for and responds to relevant system events (e.g. suspend and wake),
  * ensures the object is operating in the correct thread (on OS X it may be
  * created in a CFRunLoop, will which will not work with QEvents) and, if necessary,
  * delays object creation to allow a UI to be created and any relevant EDID to
  * be gathered (which can be critical to libCEC).
  *
  * \todo Extend to handle settings management
  *
  * \sa TorcEDID
  * \sa TorcCECDevicePriv
*/

void TorcCECDevice::Create(void)
{
    QMutexLocker locker(gCECDeviceLock);

    if (!gCECDevice)
    {
        // On OS X, creation may be triggered from the CFRunLoop rather than
        // a QThread - so check the current thread and move the object if
        // necessary, otherwise trigger initiation straight away.
        gCECDevice = new TorcCECDevice();

        QThread *adminthread = TorcThread::GetQThread(TORC_ADMIN_THREAD);

        if (TorcThread::IsCurrentThread(adminthread))
            gCECDevice->Open();
        else
            gCECDevice->moveToThread(adminthread);
    }
}

void TorcCECDevice::Destroy(void)
{
    QThread *adminthread = TorcThread::GetQThread(TORC_ADMIN_THREAD);

    QMutexLocker locker(gCECDeviceLock);

    if (gCECDevice)
    {
        if (TorcThread::IsCurrentThread(adminthread))
        {
            delete gCECDevice;
        }
        else
        {
            QEvent *event = new QEvent(QEvent::Close);
            QCoreApplication::postEvent(gCECDevice, event);
        }
    }

    gCECDevice = NULL;
}

TorcCECDevice::TorcCECDevice()
  : QObject(NULL),
    m_priv(NULL),
    m_retryCount(0),
    m_retryTimer(0)
{
}

TorcCECDevice::~TorcCECDevice()
{
    Close();
}

bool TorcCECDevice::event(QEvent *Event)
{
    QMutexLocker locker(gCECDeviceLock);

    if (Event->type() == QEvent::ThreadChange)
    {
        // the object will be moved into the admin thread so
        // schedule device inititialisation
        QEvent *event = new QEvent(QEvent::Enter);
        QCoreApplication::postEvent(this, event);
        return true;
    }
    else if (Event->type() == QEvent::Enter)
    {
        // thread affinity has been changed and the device can be opened
        Open();
        // listen for power events
        gLocalContext->AddObserver(this);
        return true;
    }
    else if (Event->type() == QEvent::Close)
    {
        delete this;
        gCECDevice = NULL;
        return true;
    }
    else if (Event->type() == TorcEvent::TorcEventType)
    {
        // close the device when suspending etc and restart on wake
        TorcEvent* torcevent = dynamic_cast<TorcEvent*>(Event);
        if (torcevent)
        {
            int event = torcevent->Event();
            switch (event)
            {
                case Torc::Suspending:
                case Torc::ShuttingDown:
                case Torc::Hibernating:
                    if (m_priv)
                        m_priv->Close(false, true);
                    break;
                case Torc::WokeUp:
                    if (m_priv)
                        m_priv->Open();
                    break;
                default:
                    break;
            }
        }
    }
    else if (Event->type() == QEvent::Timer && m_retryTimer)
    {
        QTimerEvent *timerevent = static_cast<QTimerEvent*>(Event);
        if (timerevent && (timerevent->timerId() == m_retryTimer))
        {
            killTimer(m_retryTimer);
            Open();
        }
    }

    return false;
}

void TorcCECDevice::Open(void)
{
    QMutexLocker locker(gCECDeviceLock);

    if (!TorcEDID::Ready() && (m_retryCount++ < 10))
    {
        LOG(VB_GENERAL, LOG_INFO, "EDID not ready yet - deferring CEC device startup");
        m_retryTimer = startTimer(200);
        return;
    }

    if (!m_priv)
    {
        m_priv = new TorcCECDevicePriv();
        m_priv->Open();
    }
}

void TorcCECDevice::Close(void)
{
    QMutexLocker locker(gCECDeviceLock);

    // stop the retry timer
    if (m_retryTimer)
    {
        killTimer(m_retryTimer);
        m_retryTimer = 0;
    }
    m_retryCount = 0;

    // stop listening for power events
    gLocalContext->RemoveObserver(this);

    // delete the libCEC instance
    delete m_priv;
    m_priv = NULL;
}

/*! \class TorcCECDevicehandler
  * \brief A class to detect and handle libCEC capable devices.
  *
  * \sa TorcUSB
  * \sa TorcCECDevice
*/

static class TorcCECDeviceHandler : public TorcUSBDeviceHandler
{
  public:
    bool DeviceAdded(const TorcUSBDevice &Device)
    {
        if (!(Device.m_vendorID  == 0x2548 && Device.m_productID == 0x1001))
            return false;

        if (!m_path.isEmpty())
        {
            LOG(VB_GENERAL, LOG_NOTICE, "Already have one CEC device attached - ignoring");
            return false;
        }

        if (gLocalContext->GetSetting(LIBCEC_ENABLED, LIBCEC_ENABLED_DEFAULT))
        {
            m_path = Device.m_path;
            LOG(VB_GENERAL, LOG_INFO, "CEC device added");
            TorcCECDevice::Create();
            return true;
        }
        else
        {
            LOG(VB_GENERAL, LOG_NOTICE, "libCEC support disabled - ignoring device");
        }

        return false;
    }

    bool DeviceRemoved(const TorcUSBDevice &Device)
    {
        if (Device.m_path == m_path)
        {
            TorcCECDevice::Destroy();
            LOG(VB_GENERAL, LOG_INFO, "CEC device removed");
            m_path = QString("");
            return true;
        }

        return false;
    }

  private:
    QString m_path;

} TorcCECDeviceHandler;
