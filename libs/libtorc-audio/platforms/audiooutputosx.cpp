/*****************************************************************************
 * = NAME
 * audiooutputca.cpp
 *
 * = DESCRIPTION
 * Core Audio glue for Mac OS X.
 *
 * = REVISION
 * $Id$
 *
 * = AUTHORS
 * Jeremiah Morris, Andrew Kimpton, Nigel Pearson, Jean-Yves Avenard
 *****************************************************************************/

// OS X
#include <CoreServices/CoreServices.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioFormat.h>
#include <IOKit/audio/IOAudioTypes.h>

// Torc
#include "torccoreutils.h"
#include "torclocalcontext.h"
#include "torcosxutils.h"
#include "SoundTouch.h"
#include "audiooutputosx.h"

#define CHANNELS_MIN 1
#define CHANNELS_MAX 8

QString UInt32ToFourCC(UInt32 Value)
{
    char* id = (char*)&Value;
    return QString("%1%2%3%4").arg(QChar(id[3])).arg(QChar(id[2])).arg(QChar(id[1])).arg(QChar(id[0]));
}

QString StreamDescriptionToString(AudioStreamBasicDescription Description)
{
    QString fourcc = UInt32ToFourCC(Description.mFormatID);
    QString result;

    switch (Description.mFormatID)
    {
        case kAudioFormatLinearPCM:
            result = QString("[%1] %2%3 Channel %4-bit %5 %6 (%7Hz)").arg(fourcc)
                        .arg((Description.mFormatFlags & kAudioFormatFlagIsNonMixable) ? "" : "Mixable ")
                        .arg(Description.mChannelsPerFrame).arg(Description.mBitsPerChannel)
                        .arg((Description.mFormatFlags & kAudioFormatFlagIsFloat) ? "Floating Point" : "Signed Integer")
                        .arg((Description.mFormatFlags & kAudioFormatFlagIsBigEndian) ? "BE" : "LE")
                        .arg((UInt32)Description.mSampleRate);
            break;
        case kAudioFormatAC3:
            result = QString("[%1] AC-3/DTS (%2Hz)").arg(fourcc).arg((UInt32)Description.mSampleRate);
            break;
        case kAudioFormat60958AC3:
            result = QString("[%1] AC-3/DTS for S/PDIF %2 (%3Hz)")
                        .arg(fourcc).arg((Description.mFormatFlags & kAudioFormatFlagIsBigEndian) ? "BE" : "LE")
                        .arg((UInt32)Description.mSampleRate);
            break;
        default:
            result = QString("[%1]").arg(fourcc);
            break;
    }

    return result;
}

#define COMMON_SAMPLE_RATE_COUNT 15

static Float64 CommonSampleRates[] =
{
    8000.0,   11025.0,  12000.0,
    16000.0,  22050.0,  24000.0,
    32000.0,  44100.0,  48000.0,
    64000.0,  88200.0,  96000.0,
    128000.0, 176400.0, 192000.0
};

static bool IsRateCommon(Float64 Rate)
{
    for (UInt32 i = 0; i < COMMON_SAMPLE_RATE_COUNT; i++)
        if (Rate == CommonSampleRates[i])
            return true;
    return false;
}

template <class AudioDataType>
static inline void _ReorderSmpteToCA(AudioDataType *buf, uint frames)
{
    AudioDataType tmpLS, tmpRS, tmpRLs, tmpRRs, *buf2;
    for (uint i = 0; i < frames; i++)
    {
        buf = buf2 = buf + 4;
        tmpRLs = *buf++;
        tmpRRs = *buf++;
        tmpLS = *buf++;
        tmpRS = *buf++;

        *buf2++ = tmpLS;
        *buf2++ = tmpRS;
        *buf2++ = tmpRLs;
        *buf2++ = tmpRRs;
    }
}

static inline void ReorderSmpteToCA(void *Buffer, uint Frames, AudioFormat Format)
{
    switch(AudioOutputSettings::FormatToBits(Format))
    {
        case  8: _ReorderSmpteToCA((uchar *)Buffer, Frames); break;
        case 16: _ReorderSmpteToCA((short *)Buffer, Frames); break;
        default: _ReorderSmpteToCA((int   *)Buffer, Frames); break;
    }
}

/*! \class AudioOutoutOSXPriv
 *  \brief Concrete implementation of CoreAudio interface audio output on OS X.
 *
 * \todo Action device changes when notified via callback (needs QObject/QThread changes to AudioOutput to enable proper signalling).
 * \todo Review use of usleep after audio initialisation failure.
 * \todo Look at master volume changes.
 * \todo Refactor OpenAnalog.
*/
class AudioOutputOSXPriv
{
  public:
    AudioOutputOSXPriv(AudioOutputOSX *Parent);
    AudioOutputOSXPriv(AudioOutputOSX *Parent, QString DeviceName);

    static QVector<AudioDeviceID> GetDevices     (bool Input = false);
    static int      GetTotalOutputChannels       (UInt32 DeviceID);
    static QString  GetName                      (UInt32 DeviceID);

    void            Initialise                   (void);
    bool            Open                         (bool Digital);
    void            Close                        (void);
    AudioDeviceID   GetDefaultOutputDevice       (void);
    bool            GetAutoHogMode               (void);
    void            SetAutoHogMode               (bool Enable);
    pid_t           GetHogStatus                 (void);
    bool            SetHogStatus                 (bool Hog);
    AudioDeviceID   GetDeviceWithName            (QString DeviceName);
    int             OpenAnalog                   (void);
    static OSStatus RenderCallbackAnalog         (void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp,
                                                  UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData);
    void            CloseAnalog                  (void);
    bool            OpenDigital                  (void);
    static OSStatus RenderCallbackDigital        (AudioDeviceID, const AudioTimeStamp*, const void*, const AudioTimeStamp*,
                                                  AudioBufferList *outOutputData, const AudioTimeStamp *inOutputTime, void *inRefCon);
    void            CloseDigital                 (void);
    bool            SetMixingSupport             (bool Mix);
    bool            GetMixingSupport             (void);
    bool            FindAC3Stream                (void);
    void            ResetAudioDevices            (void);
    void            ResetStream                  (AudioStreamID StreamID);
    QList<int>      GetSampleRates               (AudioDeviceID DeviceID);
    QList<int>      GetChannelsList              (AudioDeviceID, bool Passthrough);
    UInt32          GetTerminalType              (AudioStreamID StreamID);
    UInt32          GetTransportType             (AudioDeviceID DeviceID);
    QVector<AudioStreamID> GetStreamsList        (AudioDeviceID DeviceID);
    QVector<AudioStreamBasicDescription> GetFormatsList (AudioStreamID StreamID);
    int             ChangeAudioStreamFormat      (AudioStreamID StreamID, AudioStreamBasicDescription Format);
    static OSStatus DefaultDeviceChangedCallback (AudioObjectID ObjectID, UInt32 NumberAddresses,
                                                  const AudioObjectPropertyAddress Addresses[], void* Client);
    static OSStatus DeviceIsAliveCallback        (AudioObjectID ObjectID, UInt32 NumberAddresses,
                                                  const AudioObjectPropertyAddress Addresses[], void* Client);
    static OSStatus DeviceListChangedCallback    (AudioObjectID ObjectID, UInt32 NumberAddresses,
                                                  const AudioObjectPropertyAddress Addresses[], void* Client);

  public:
    AudioOutputOSX *m_parent;

    // Analog
    AudioUnit       m_analogAudioUnit;

    // Digital
    bool            m_digitalInUse;
    pid_t           m_hog;
    int             m_restoreMixer;
    AudioDeviceID   m_deviceID;
    AudioStreamID   m_streamID;     // StreamID that has a cac3 streamformat
    AudioDeviceIOProcID m_spdifIOProcID;
    int             m_streamIndex;  // Index of m_streamID in an AudioBufferList
    UInt32          m_bytesPerPacket;
    AudioStreamBasicDescription m_formatOrig;
    AudioStreamBasicDescription m_formatNew;
    bool            m_revertFormat; // Do we need to revert the stream format?
    bool            m_hasCallback;
    bool            m_initialised;
    bool            m_started;
    bool            m_wasDigital;
};

AudioOutputOSXPriv::AudioOutputOSXPriv(AudioOutputOSX *Parent)
  : m_parent(Parent)
{
    Initialise();
    ResetAudioDevices();
    m_deviceID = GetDefaultOutputDevice();
}

AudioOutputOSXPriv::AudioOutputOSXPriv(AudioOutputOSX *Parent, QString DeviceName)
  : m_parent(Parent)
{
    Initialise();
    ResetAudioDevices();
    m_deviceID = GetDeviceWithName(DeviceName);

    if (!m_deviceID)
    {
        // Didn't find specified device, use default one
        m_deviceID = GetDefaultOutputDevice();
        if (DeviceName != "Default")
        {
            LOG(VB_GENERAL, LOG_WARNING, QString("'%1' not found, using default device '%2'")
                 .arg(DeviceName).arg(m_deviceID));
        }
    }

    LOG(VB_AUDIO, LOG_INFO, QString("Device number '%1'").arg(m_deviceID));
}

void AudioOutputOSXPriv::Initialise(void)
{
    m_analogAudioUnit = NULL;
    m_deviceID        = 0;
    m_digitalInUse    = false;
    m_revertFormat    = false;
    m_hog             = -1;
    m_restoreMixer    = -1;
    m_streamIndex     = -1;
    m_hasCallback     = false;
    m_initialised     = false;
    m_started         = false;
    m_bytesPerPacket  = -1;
    m_wasDigital      = false;
    m_spdifIOProcID   = 0;
}

bool AudioOutputOSXPriv::Open(bool Digital)
{
    if (Digital)
    {
        LOG(VB_AUDIO, LOG_DEBUG, "Trying digital output");
        if (OpenDigital())
            return true;
        else
            CloseDigital();
    }

    int result = OpenAnalog();
    LOG(VB_AUDIO, LOG_DEBUG, QString("Trying analog output (Result: %1)").arg(result));

    if (result < 0)
    {
        CloseAnalog();
        LOG(VB_GENERAL, LOG_ERR, "Failed to open audio device");
        QThread::usleep(1000000 - 1);
        return false;
    }

    AudioObjectPropertyAddress propertyAddress;
    propertyAddress.mScope    = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement  = kAudioObjectPropertyElementMaster;

    // register for changes in the default output device
    propertyAddress.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
    if (AudioObjectAddPropertyListener(kAudioObjectSystemObject, &propertyAddress, DefaultDeviceChangedCallback, this) != noErr)
        LOG(VB_GENERAL, LOG_ERR, "Failed to listen for default device changes");

    // listen for 'alive' status of outpout device
    if (m_deviceID)
    {
        propertyAddress.mSelector = kAudioDevicePropertyDeviceIsAlive;
        if (AudioObjectAddPropertyListener(kAudioObjectSystemObject, &propertyAddress, DeviceIsAliveCallback, this) != noErr)
            LOG(VB_GENERAL, LOG_ERR, "Failed to listen for device 'alive' changes");
    }

    // listen for changes in the device list
    propertyAddress.mSelector = kAudioHardwarePropertyDevices;
    if (AudioObjectAddPropertyListener(kAudioObjectSystemObject, &propertyAddress, DeviceListChangedCallback, this) != noErr)
        LOG(VB_GENERAL, LOG_ERR, "Failed to listen for device list changes");

    return true;
}

void AudioOutputOSXPriv::Close(void)
{
    AudioObjectPropertyAddress propertyAddress;
    propertyAddress.mScope    = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement  = kAudioObjectPropertyElementMaster;

    // stop listening for default device changes
    propertyAddress.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
    AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &propertyAddress, DefaultDeviceChangedCallback, this);

    // stop listening for 'alive' status changes
    propertyAddress.mSelector = kAudioDevicePropertyDeviceIsAlive;
    AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &propertyAddress, DeviceIsAliveCallback, this);

    // stop listening for device list changes
    propertyAddress.mSelector = kAudioHardwarePropertyDevices;
    AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &propertyAddress, DeviceListChangedCallback, this);

    if (m_digitalInUse)
        CloseDigital();
    else
        CloseAnalog();
}

AudioDeviceID AudioOutputOSXPriv::GetDefaultOutputDevice(void)
{
    AudioDeviceID deviceID = 0;

    AudioObjectPropertyAddress propertyAddress;
    UInt32 propertySize       = sizeof(AudioDeviceID);
    propertyAddress.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
    propertyAddress.mScope    = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement  = kAudioObjectPropertyElementMaster;
    OSStatus err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &propertySize, &deviceID);

    if (err == noErr)
    {
        LOG(VB_AUDIO, LOG_INFO, QString("Default device ID: '%1' (%2)").arg(deviceID).arg(GetName(deviceID)));
    }
    else
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("Failed to get default audio device '%1'").arg(UInt32ToFourCC(err)));
        deviceID = 0;
    }

    return deviceID;
}

int AudioOutputOSXPriv::GetTotalOutputChannels(UInt32 DeviceID)
{
    AudioObjectPropertyAddress propertyAddress;
    propertyAddress.mSelector = kAudioDevicePropertyStreamConfiguration;
    propertyAddress.mScope    = kAudioObjectPropertyScopeOutput;
    propertyAddress.mElement  = kAudioObjectPropertyElementMaster;

    if (AudioObjectHasProperty(DeviceID, &propertyAddress))
    {
        UInt32 size = 0;
        OSStatus err = AudioObjectGetPropertyDataSize(DeviceID, &propertyAddress, 0, NULL, &size);

        if ((err == noErr) && size)
        {
            QByteArray buffer(size, 0);
            err = AudioObjectGetPropertyData(DeviceID, &propertyAddress, 0, NULL, &size, buffer.data());

            AudioBufferList *list = reinterpret_cast<AudioBufferList*>(buffer.data());
            if ((err == noErr) && list)
            {
                UInt32 channels = 0;
                for (UInt32 i = 0; i < list->mNumberBuffers; ++i)
                    channels += list->mBuffers[i].mNumberChannels;
                LOG(VB_AUDIO, LOG_INFO, QString("'%1': Channels %2 Buffers %3").arg(GetName(DeviceID)).arg(channels).arg(list->mNumberBuffers));
                return channels;
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Failed to retrieve stream configuration (Error: %1)").arg(UInt32ToFourCC(err)));
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to retrieve stream configuration (Error: %1)").arg(UInt32ToFourCC(err)));
        }
    }

    LOG(VB_GENERAL, LOG_WARNING, "Unable to get total device output channels for device");
    return 0;
}

QString AudioOutputOSXPriv::GetName(UInt32 DeviceID)
{
    CFStringRef name;
    UInt32 propertySize       = sizeof(CFStringRef);

    AudioObjectPropertyAddress propertyAddress;
    propertyAddress.mSelector = kAudioObjectPropertyName;
    propertyAddress.mScope    = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement  = kAudioObjectPropertyElementMaster;
    OSStatus err = AudioObjectGetPropertyData(DeviceID, &propertyAddress, 0, NULL, &propertySize, &name);

    if (err != noErr)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to retrieve object name (Error: %1)").arg(err));
        return QString();
    }

    return CFStringReftoQString(name);
}

bool AudioOutputOSXPriv::GetAutoHogMode(void)
{
    UInt32 hogmode;
    UInt32 propertySize       = sizeof(UInt32);

    AudioObjectPropertyAddress propertyAddress;
    propertyAddress.mSelector = kAudioHardwarePropertyHogModeIsAllowed;
    propertyAddress.mScope    = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement  = kAudioObjectPropertyElementMaster;
    OSStatus err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &propertySize, &hogmode);

    if (err != noErr)
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("Failed to get auto 'hog' mode (Error: %1)").arg(err));
        return false;
    }

    return (hogmode == 1);
}

void AudioOutputOSXPriv::SetAutoHogMode(bool Enable)
{
    UInt32 hogmode            = Enable ? 1 : 0;
    UInt32 propertySize       = sizeof(UInt32);

    AudioObjectPropertyAddress propertyAddress;
    propertyAddress.mSelector = kAudioDevicePropertyHogMode;
    propertyAddress.mScope    = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement  = kAudioObjectPropertyElementMaster;
    OSStatus err = AudioObjectSetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, propertySize, &hogmode);

    if (err != noErr)
        LOG(VB_GENERAL, LOG_WARNING, QString("Failed to set auto 'hog'mode (Error: %1)").arg(err));
}

pid_t AudioOutputOSXPriv::GetHogStatus(void)
{
    pid_t pid;
    UInt32 pidsize = sizeof(pid);

    AudioObjectPropertyAddress propertyAddress;
    propertyAddress.mSelector = kAudioDevicePropertyHogMode;
    propertyAddress.mScope    = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement  = kAudioObjectPropertyElementMaster;
    OSStatus err = AudioObjectGetPropertyData(m_deviceID, &propertyAddress, 0, NULL, &pidsize, &pid);

    if (err != noErr)
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("Failed to get auto 'hog' status (Error: %1)").arg(err));
        return -1;
    }

    return pid;
}

bool AudioOutputOSXPriv::SetHogStatus(bool Hog)
{
    // According to Jeff Moore (Core Audio, Apple), Setting kAudioDevicePropertyHogMode
    // is a toggle and the only way to tell if you do get hog mode is to compare
    // the returned pid against getpid, if the match, you have hog mode, if not you don't.
    if (!m_deviceID)
        return false;

    AudioObjectPropertyAddress propertyAddress;
    propertyAddress.mSelector = kAudioDevicePropertyHogMode;
    propertyAddress.mScope    = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement  = kAudioObjectPropertyElementMaster;

    if (Hog)
    {
        if (m_hog == -1)
        {
            LOG(VB_AUDIO, LOG_INFO, QString("Setting 'hog' status on device '%1'").arg(m_deviceID));

            OSStatus err = AudioObjectSetPropertyData(m_deviceID, &propertyAddress, 0, NULL, sizeof(m_hog), &m_hog);

            if (err || m_hog != getpid())
            {
                LOG(VB_GENERAL, LOG_WARNING, QString("Unable to set 'hog' status (Error: %1)").arg(UInt32ToFourCC(err)));
                return false;
            }

            LOG(VB_AUDIO, LOG_INFO, QString("Set 'hog' status on device '%1'").arg(m_deviceID));
        }
    }
    else
    {
        if (m_hog > -1)
        {
            LOG(VB_AUDIO, LOG_INFO, QString("Releasing 'hog' status on device '%1'").arg(m_deviceID));
            pid_t hogpid = -1;

            OSStatus err = AudioObjectSetPropertyData(m_deviceID, &propertyAddress, 0, NULL, sizeof(hogpid), &hogpid);

            if (err || hogpid == getpid())
            {
                LOG(VB_GENERAL, LOG_WARNING, QString("Unable to release 'hog' status (Error: %1)").arg(UInt32ToFourCC(err)));
                return false;
            }

            m_hog = hogpid;
        }
    }

    return true;
}

QVector<AudioDeviceID> AudioOutputOSXPriv::GetDevices(bool Input /*=false*/)
{
    AudioObjectPropertyAddress propertyAddress;
    propertyAddress.mSelector = kAudioHardwarePropertyDevices;
    propertyAddress.mScope    = Input ? kAudioObjectPropertyScopeInput : kAudioObjectPropertyScopeOutput;
    propertyAddress.mElement  = kAudioObjectPropertyElementMaster;
    UInt32 size = 0;

    OSStatus err = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &size);

    if (err == noErr && (size >= sizeof(AudioDeviceID)))
    {
        UInt32 devicecount = size / sizeof(AudioDeviceID);
        QVector<AudioDeviceID> devices(devicecount);

        err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &size, devices.data());

        if (err == noErr)
            return devices;
    }

    LOG(VB_GENERAL, LOG_ERR, "Failed to retrieve audio device list");
    return QVector<AudioDeviceID>();
}

AudioDeviceID AudioOutputOSXPriv::GetDeviceWithName(QString DeviceName)
{
    QVector<AudioDeviceID> devices = GetDevices();

    if (!devices.isEmpty())
    {
        foreach (AudioDeviceID deviceid, devices)
        {
            if (GetTotalOutputChannels(deviceid) == 0)
                continue;

            QString name = GetName(deviceid);
            if (!name.isEmpty() && name == DeviceName)
            {
                LOG(VB_AUDIO, LOG_INFO, QString("Found: %1").arg(name));
                return deviceid;
            }
        }
    }

    return 0;
}

int AudioOutputOSXPriv::OpenAnalog(void)
{
    LOG(VB_AUDIO, LOG_INFO, "OpenAnalog");

    AudioDeviceID defaultDevice = GetDefaultOutputDevice();

    ComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;

    if (defaultDevice == m_deviceID)
    {
        desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    }
    else
    {
        desc.componentSubType = kAudioUnitSubType_HALOutput;
    }

    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    m_digitalInUse = false;

    Component comp = FindNextComponent(NULL, &desc);
    if (comp == NULL)
    {
        LOG(VB_GENERAL, LOG_ERR, "AudioComponentFindNext failed");
        return false;
    }

    OSErr err = OpenAComponent(comp, &m_analogAudioUnit);
    if (err != noErr)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("AudioComponentInstanceNew returned %1").arg(err));
        return false;
    }

    // Check if we have IO
    UInt32 hasio     = 0;
    UInt32 sizehasio = sizeof(hasio);
    err = AudioUnitGetProperty(m_analogAudioUnit, kAudioOutputUnitProperty_HasIO,
                               kAudioUnitScope_Output, 0, &hasio, &sizehasio);
    LOG(VB_AUDIO, LOG_INFO, QString("HasIO (output) = %1").arg(hasio));

    if (!hasio)
    {
        UInt32 enableIO = 1;
        err = AudioUnitSetProperty(m_analogAudioUnit, kAudioOutputUnitProperty_EnableIO,
                                   kAudioUnitScope_Global, 0, &enableIO, sizeof(enableIO));
        if (err != noErr)
        {
            LOG(VB_GENERAL, LOG_WARNING, QString("Failed enabling IO: %1")
                 .arg(err));
        }

        hasio = 0;
        err = AudioUnitGetProperty(m_analogAudioUnit, kAudioOutputUnitProperty_HasIO,
                                   kAudioUnitScope_Output, 0, &hasio, &sizehasio);
        LOG(VB_AUDIO, LOG_INFO, QString("HasIO = %1").arg(hasio));
    }

    // We shouldn't have to do this distinction, however for some unknown reasons
    // assigning device to AudioUnit fail when switching from SPDIF mode
    if (defaultDevice != m_deviceID)
    {
        err = AudioUnitSetProperty(m_analogAudioUnit, kAudioOutputUnitProperty_CurrentDevice,
                                   kAudioUnitScope_Global, 0, &m_deviceID, sizeof(m_deviceID));
        if (err)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Unable to set current device to %1. Error = %2")
                .arg(m_deviceID).arg(err));
            return -1;
        }
    }

    // Get the current format
    AudioStreamBasicDescription deviceformat;
    UInt32 paramsize = sizeof(AudioStreamBasicDescription);

    err = AudioUnitGetProperty(m_analogAudioUnit, kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Input, 0, &deviceformat, &paramsize);
    if (err != noErr)
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("Unable to retrieve current stream format: [%1]").arg(err));
    }
    else
    {
        LOG(VB_AUDIO, LOG_INFO, QString("Current format is: %1")
              .arg(StreamDescriptionToString(deviceformat)));
    }

    // Get the channel layout of the device side of the unit
    err = AudioUnitGetPropertyInfo(m_analogAudioUnit, kAudioDevicePropertyPreferredChannelLayout,
                                   kAudioUnitScope_Output, 0, &paramsize, NULL);

    if (err == noErr)
    {
        AudioChannelLayout *layout = (AudioChannelLayout *) malloc(paramsize);

        err = AudioUnitGetProperty(m_analogAudioUnit, kAudioDevicePropertyPreferredChannelLayout,
                                   kAudioUnitScope_Output, 0, layout, &paramsize);

        // We need to "fill out" the ChannelLayout, because there are multiple ways that it can be set
        if (layout->mChannelLayoutTag == kAudioChannelLayoutTag_UseChannelBitmap)
        {
            // bitmap defined channellayout
            err = AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutForBitmap,
                                         sizeof(UInt32), &layout->mChannelBitmap,
                                         &paramsize,
                                         layout);
            if (err != noErr)
                LOG(VB_GENERAL, LOG_WARNING, "Can't retrieve current channel layout");
        }
        else if(layout->mChannelLayoutTag != kAudioChannelLayoutTag_UseChannelDescriptions )
        {
            // layouttags defined channellayout
            err = AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutForTag,
                                         sizeof(AudioChannelLayoutTag),
                                         &layout->mChannelLayoutTag,
                                         &paramsize, layout);
            if (err != noErr)
                LOG(VB_GENERAL, LOG_WARNING, "Can't retrieve current channel layout");
        }

        LOG(VB_AUDIO, LOG_INFO, QString("Layout of AUHAL has %1 channels")
            .arg(layout->mNumberChannelDescriptions));

        int channelsfound = 0;
        for (UInt32 i = 0; i < layout->mNumberChannelDescriptions; i++)
        {
            QString description = "Unknown";
            switch (layout->mChannelDescriptions[i].mChannelLabel)
            {
                case kAudioChannelLabel_Unknown:           description = "Unspecified"; break;
                case kAudioChannelLabel_Unused:            description = "Unused"; break;
                case kAudioChannelLabel_Left:              description = "Left"; break;
                case kAudioChannelLabel_Right:             description = "Right"; break;
                case kAudioChannelLabel_Center:            description = "Center"; break;
                case kAudioChannelLabel_LFEScreen:         description = "LFEScreen"; break;
                case kAudioChannelLabel_LeftSurround:      description = "LeftSurround"; break;
                case kAudioChannelLabel_RightSurround:     description = "RightSurround"; break;
                case kAudioChannelLabel_RearSurroundLeft:  description = "RearSurroundLeft"; break;
                case kAudioChannelLabel_RearSurroundRight: description = "RearSurroundRight"; break;
                case kAudioChannelLabel_CenterSurround:    description = "CenterSurround"; break;
                default:                                   break;
            }

            if (description != "Unknown")
                channelsfound++;

            LOG(VB_AUDIO, LOG_INFO, QString("This is channel: %1").arg(description));
        }

        if (channelsfound == 0)
        {
            LOG(VB_GENERAL, LOG_WARNING, "Audio device is not configured. "
                "You should configure your speaker layout with "
                "the 'Audio Midi Setup utility in /Applications/Utilities.");
        }

        free(layout);
    }
    else
    {
        LOG(VB_GENERAL, LOG_WARNING, "This driver does not support kAudioDevicePropertyPreferredChannelLayout.");
    }

    AudioChannelLayout newlayout;
    memset(&newlayout, 0, sizeof(newlayout));

    switch (m_parent->m_channels)
    {
        case 1:
            newlayout.mChannelLayoutTag = kAudioChannelLayoutTag_Mono;
            break;
        case 2:
            newlayout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
            break;
        case 6:
            //  3F2-LFE        L   R   C    LFE  LS   RS
            newlayout.mChannelLayoutTag = kAudioChannelLayoutTag_AudioUnit_5_1;
        case 8:
            // We need
            // 3F4-LFE        L   R   C    LFE  Rls  Rrs  LS   RS
            // but doesn't exist, so we'll swap channels later
            newlayout.mChannelLayoutTag = kAudioChannelLayoutTag_MPEG_7_1_A; // L R C LFE Ls Rs Lc Rc
            break;
    }

    // Set newlayout as the layout
    err = AudioUnitSetProperty(m_analogAudioUnit, kAudioUnitProperty_AudioChannelLayout,
                               kAudioUnitScope_Input, 0, &newlayout, sizeof(newlayout));
    if (err != noErr)
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("OpenAnalog: couldn't set channels layout [%1]").arg(err));
    }

    if(newlayout.mNumberChannelDescriptions > 0)
        free(newlayout.mChannelDescriptions);

    // Set up the audio output unit
    int formatFlags;
    switch (m_parent->m_outputFormat)
    {
        case FORMAT_S16:
            formatFlags = kLinearPCMFormatFlagIsSignedInteger;
            break;
        case FORMAT_FLT:
            formatFlags = kLinearPCMFormatFlagIsFloat;
            break;
        default:
            formatFlags = kLinearPCMFormatFlagIsSignedInteger;
            break;
    }

    AudioStreamBasicDescription conv_in_desc;
    memset(&conv_in_desc, 0, sizeof(AudioStreamBasicDescription));
    conv_in_desc.mSampleRate       = m_parent->m_samplerate;
    conv_in_desc.mFormatID         = kAudioFormatLinearPCM;
    conv_in_desc.mFormatFlags      = formatFlags | kAudioFormatFlagsNativeEndian | kLinearPCMFormatFlagIsPacked;
    conv_in_desc.mBytesPerPacket   = m_parent->m_outputBytesPerFrame;
    // This seems inefficient, does it hurt if we increase this?
    conv_in_desc.mFramesPerPacket  = 1;
    conv_in_desc.mBytesPerFrame    = m_parent->m_outputBytesPerFrame;
    conv_in_desc.mChannelsPerFrame = m_parent->m_channels;
    conv_in_desc.mBitsPerChannel   = AudioOutputSettings::FormatToBits(m_parent->m_outputFormat);

    // Set AudioUnit input format
    err = AudioUnitSetProperty(m_analogAudioUnit, kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Input, 0, &conv_in_desc, sizeof(AudioStreamBasicDescription));

    if (err != noErr)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("AudioUnitSetProperty returned [%1]").arg(err));
        return false;
    }

    LOG(VB_AUDIO, LOG_INFO, QString("Set format as %1").arg(StreamDescriptionToString(conv_in_desc)));
    // Retrieve actual format
    err = AudioUnitGetProperty(m_analogAudioUnit, kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Input, 0, &deviceformat, &paramsize);

    LOG(VB_AUDIO, LOG_INFO, QString("The actual set AU format is %1")
        .arg(StreamDescriptionToString(deviceformat)));

    // Attach callback to default output
    AURenderCallbackStruct input;
    input.inputProc = RenderCallbackAnalog;
    input.inputProcRefCon = this;

    err = AudioUnitSetProperty(m_analogAudioUnit, kAudioUnitProperty_SetRenderCallback,
                               kAudioUnitScope_Input, 0, &input, sizeof(input));
    if (err != noErr)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("AudioUnitSetProperty (callback) returned [%1]").arg(err));
        return false;
    }

    m_hasCallback = true;

    // We're all set up - start the audio output unit
    ComponentResult res = AudioUnitInitialize(m_analogAudioUnit);
    if (res)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("AudioUnitInitialize error: [%1]").arg(res));
        return false;
    }

    m_initialised = true;

    err = AudioOutputUnitStart(m_analogAudioUnit);
    if (err != noErr)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("AudioOutputUnitStart error: [%1]").arg(err));
        return false;
    }

    m_started = true;
    return true;
}

OSStatus AudioOutputOSXPriv::RenderCallbackAnalog(void                       *inRefCon,
                                                  AudioUnitRenderActionFlags *ioActionFlags,
                                                  const AudioTimeStamp       *inTimeStamp,
                                                  UInt32                      inBusNumber,
                                                  UInt32                      inNumberFrames,
                                                  AudioBufferList            *ioData)
{
    (void)inBusNumber;
    (void)inNumberFrames;

    AudioOutputOSX *inst = (static_cast<AudioOutputOSXPriv *>(inRefCon))->m_parent;

    if (!inst->RenderAudio((unsigned char *)(ioData->mBuffers[0].mData),
                           ioData->mBuffers[0].mDataByteSize,
                           inTimeStamp->mHostTime))
    {
        // play silence if RenderAudio returns false
        memset(ioData->mBuffers[0].mData, 0, ioData->mBuffers[0].mDataByteSize);
        *ioActionFlags = kAudioUnitRenderAction_OutputIsSilence;
    }
    return noErr;
}

void AudioOutputOSXPriv::CloseAnalog(void)
{
    LOG(VB_GENERAL, LOG_INFO, "Closing analog");

    if (m_analogAudioUnit)
    {
        if (m_started)
        {
            if (AudioOutputUnitStop(m_analogAudioUnit) != noErr)
                LOG(VB_AUDIO, LOG_INFO, "Failed to stop audio");
        }

        if (m_initialised)
        {
            if (AudioUnitUninitialize(m_analogAudioUnit) != noErr)
                LOG(VB_AUDIO, LOG_INFO, "Failed to uninitialise audio");
        }

        if (CloseComponent(m_analogAudioUnit) != noErr)
            LOG(VB_AUDIO, LOG_INFO, "Failed to close audio component");

        m_analogAudioUnit = NULL;
    }

    m_hasCallback      = false;
    m_initialised = false;
    m_started     = false;
    m_wasDigital  = false;
}

bool AudioOutputOSXPriv::OpenDigital(void)
{
    OSStatus       err;
    AudioStreamBasicDescription outputFormat = {0};

    LOG(VB_GENERAL, LOG_INFO, "Open digital");

    QVector<AudioStreamID> streams = GetStreamsList(m_deviceID);
    if (streams.isEmpty())
    {
        LOG(VB_GENERAL, LOG_WARNING, "Couldn't retrieve list of streams");
        return false;
    }

    for (int i = 0; i < streams.size(); ++i)
    {
        LOG(VB_GENERAL, LOG_INFO, QString::number(i));
        QVector<AudioStreamBasicDescription> formats = GetFormatsList(streams[i]);
        if (formats.isEmpty())
            continue;

        foreach (AudioStreamBasicDescription format, formats)
        {
            LOG(VB_AUDIO, LOG_INFO, QString("Considering Physical Format: %1")
                  .arg(StreamDescriptionToString(format)));

            if ((format.mFormatID == 'IAC3' || format.mFormatID == kAudioFormat60958AC3) &&
                format.mSampleRate == m_parent->m_samplerate)
            {
                LOG(VB_AUDIO, LOG_INFO, "Found digital format");
                m_streamIndex  = i;
                m_streamID     = streams[i];
                outputFormat  = format;
                break;
            }
        }

        if (outputFormat.mFormatID)
            break;
    }

    if (!outputFormat.mFormatID)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Couldn't find suitable output"));
        return false;
    }

    if (m_revertFormat == false)
    {
        // Retrieve the original format of this stream first if not done so already
        AudioObjectPropertyAddress propertyAddress;
        propertyAddress.mSelector = kAudioStreamPropertyPhysicalFormat;
        propertyAddress.mScope    = kAudioObjectPropertyScopeGlobal;
        propertyAddress.mElement  = kAudioObjectPropertyElementMaster;
        UInt32 size = sizeof(m_formatOrig);

        err = AudioObjectGetPropertyData(m_streamID, &propertyAddress, 0, NULL, &size, &m_formatOrig);

        if (err != noErr)
            LOG(VB_GENERAL, LOG_WARNING, QString("Failed to retrieve the original streamformat (Error: %1)").arg(UInt32ToFourCC(err)));
        else
            m_revertFormat = true;
    }

    m_digitalInUse = true;

    SetAutoHogMode(false);
    bool autoHog = GetAutoHogMode();
    if (!autoHog)
    {
        SetHogStatus(true);
        // Set mixable to false if we are allowed to
        SetMixingSupport(false);
    }

    m_formatNew = outputFormat;
    if (!ChangeAudioStreamFormat(m_streamID, m_formatNew))
        return false;

    m_bytesPerPacket = m_formatNew.mBytesPerPacket;

    err = AudioDeviceCreateIOProcID(m_deviceID, (AudioDeviceIOProc)RenderCallbackDigital, (void*)this, &m_spdifIOProcID);

    if (err != noErr)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("AudioDeviceAddIOProc failed: [%1]")
              .arg(UInt32ToFourCC(err)));
        return false;
    }

    m_hasCallback = true;
    err = AudioDeviceStart(m_deviceID, m_spdifIOProcID);

    if (err != noErr)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("AudioDeviceStart failed: [%1]")
              .arg(UInt32ToFourCC(err)));
        return false;
    }

    m_started = true;
    return true;
}

OSStatus AudioOutputOSXPriv::RenderCallbackDigital(AudioDeviceID, const AudioTimeStamp*, const void*, const AudioTimeStamp*,
                                                   AudioBufferList *outOutputData, const AudioTimeStamp *inOutputTime, void *inRefCon)
{
    AudioOutputOSXPriv *priv = static_cast<AudioOutputOSXPriv *>(inRefCon);
    AudioOutputOSX    *audio = priv->m_parent;
    int index = priv->m_streamIndex;

    /*
     * HACK: No idea why this would be the case, but after the second run, we get
     * incorrect value
     */
    if (priv->m_bytesPerPacket > 0 && outOutputData->mBuffers[index].mDataByteSize > priv->m_bytesPerPacket)
        outOutputData->mBuffers[index].mDataByteSize = priv->m_bytesPerPacket;

    if (!audio->RenderAudio((unsigned char *)(outOutputData->mBuffers[index].mData), outOutputData->mBuffers[index].mDataByteSize, inOutputTime->mHostTime))
        memset(outOutputData->mBuffers[index].mData, 0, outOutputData->mBuffers[index].mDataByteSize);

    return noErr;
}

void AudioOutputOSXPriv::CloseDigital(void)
{
    if (!m_digitalInUse)
        return;

    LOG(VB_GENERAL, LOG_INFO, "Closing digital");

    if (m_started)
    {
        OSStatus err = AudioDeviceStop(m_deviceID, NULL);
        if (err != noErr)
            LOG(VB_GENERAL, LOG_ERR, QString("AudioDeviceStop failed (Error: %1)").arg(UInt32ToFourCC(err)));

        m_started = false;
    }

    if (m_hasCallback || m_spdifIOProcID)
    {
        OSStatus err = AudioDeviceDestroyIOProcID(m_deviceID, m_spdifIOProcID);

        if (err != noErr)
            LOG(VB_GENERAL, LOG_ERR, QString("AudioDeviceRemoveIOProc failed (Error: %1)").arg(UInt32ToFourCC(err)));

        m_spdifIOProcID = 0;
        m_hasCallback = false;
    }

    if (m_revertFormat)
    {
        ChangeAudioStreamFormat(m_streamID, m_formatOrig);
        m_revertFormat = false;
    }

    SetHogStatus(false);

    if (m_restoreMixer > -1) // We changed the mixer status
        SetMixingSupport((m_restoreMixer ? true : false));

    AudioHardwareUnload();
    m_restoreMixer   = -1;
    m_bytesPerPacket = -1;
    m_streamIndex    = -1;
    m_wasDigital     = true;
}

bool AudioOutputOSXPriv::SetMixingSupport(bool Mix)
{
    if (!m_deviceID)
        return false;

    int restore = -1;
    if (m_restoreMixer == -1)
        restore = (GetMixingSupport() ? 1 : 0);

    UInt32 enablemixing = Mix ? 1 : 0;
    LOG(VB_AUDIO, LOG_INFO, QString("%1abling mixing for device '%2'").arg(Mix ? "En" : "Dis").arg(m_deviceID));

    AudioObjectPropertyAddress propertyAddress;
    propertyAddress.mSelector = kAudioDevicePropertySupportsMixing;
    propertyAddress.mScope    = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement  = kAudioObjectPropertyElementMaster;

    OSStatus err = AudioObjectSetPropertyData(m_deviceID, &propertyAddress, 0, NULL, sizeof(enablemixing), &enablemixing);

    if (err != noErr)
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("Unable to set MixingSupport to %1. Error = [%2]")
            .arg(Mix ? "'On'" : "'Off'").arg(UInt32ToFourCC(err)));
        return false;
    }

    if (m_restoreMixer == -1)
        m_restoreMixer = restore;

    return true;
}

bool AudioOutputOSXPriv::GetMixingSupport(void)
{
    if (!m_deviceID)
        return false;

    AudioObjectPropertyAddress propertyAddress;
    propertyAddress.mSelector = kAudioDevicePropertySupportsMixing;
    propertyAddress.mScope    = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement  = kAudioObjectPropertyElementMaster;
    UInt32 value;
    UInt32 size = sizeof(UInt32);

    OSStatus err = AudioObjectGetPropertyData(m_deviceID, &propertyAddress, 0, NULL, &size, &value);

    if (err == noErr)
        return value > 0;

    return false;
}

bool AudioOutputOSXPriv::FindAC3Stream(void)
{
    bool found = false;

    QVector<AudioStreamID> streams = GetStreamsList(m_deviceID);
    if (streams.isEmpty())
        return found;

    foreach (AudioStreamID stream, streams)
    {
        QVector<AudioStreamBasicDescription> formats = GetFormatsList(stream);
        if (formats.isEmpty())
            continue;

        foreach (AudioStreamBasicDescription format, formats)
        {
            if (format.mFormatID == 'IAC3' || format.mFormatID == kAudioFormat60958AC3)
            {
                LOG(VB_AUDIO, LOG_INFO, "FindAC3Stream: found digital format");
                found = true;
                break;
            }
        }
    }

    return found;
}

void AudioOutputOSXPriv::ResetAudioDevices(void)
{
    QVector<AudioDeviceID> devices = GetDevices();

    foreach (AudioDeviceID deviceid, devices)
    {
        QVector<AudioStreamID> streams = GetStreamsList(deviceid);
        foreach (AudioStreamID stream, streams)
            ResetStream(stream);
    }
}

void AudioOutputOSXPriv::ResetStream(AudioStreamID StreamID)
{
    AudioStreamBasicDescription  currentformat;
    AudioObjectPropertyAddress propertyAddress;
    propertyAddress.mSelector = kAudioStreamPropertyPhysicalFormat;
    propertyAddress.mScope    = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement  = kAudioObjectPropertyElementMaster;
    UInt32 size = sizeof(currentformat);

    OSStatus err = AudioObjectGetPropertyData(StreamID, &propertyAddress, 0, NULL, &size, &currentformat);

    // If it's currently AC-3/SPDIF then reset it to some mixable format
    if (currentformat.mFormatID == 'IAC3' || currentformat.mFormatID == kAudioFormat60958AC3)
    {
        QVector<AudioStreamBasicDescription> formats = GetFormatsList(StreamID);
        bool streamreset = false;
        if (formats.isEmpty())
            return;

        foreach (AudioStreamBasicDescription format, formats)
        {
            if (!streamreset && format.mFormatID == kAudioFormatLinearPCM)
            {
                err = AudioObjectSetPropertyData(StreamID, &propertyAddress, 0, NULL, size, &format);

                if (err != noErr)
                {
                    LOG(VB_GENERAL, LOG_WARNING, QString("Could not reset physical format (Error: %1)").arg(UInt32ToFourCC(err)));
                    continue;
                }
                else
                {
                    streamreset = true;
                    // Is this really needed?
                    QThread::sleep(1);
                }
            }
        }
    }
}

QList<int> AudioOutputOSXPriv::GetSampleRates(AudioDeviceID DeviceID)
{
    QList<int> results;

    UInt32 size = sizeof(UInt32);
    AudioObjectPropertyAddress propertyAddress;
    propertyAddress.mSelector = kAudioDevicePropertyAvailableNominalSampleRates;
    propertyAddress.mScope    = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement  = kAudioObjectPropertyElementMaster;

    OSStatus err = AudioObjectGetPropertyDataSize(DeviceID, &propertyAddress, 0, NULL, &size);
    if (err == noErr)
    {
        QVector<AudioValueRange> list(size / sizeof(AudioValueRange));
        err = AudioObjectGetPropertyData(DeviceID, &propertyAddress, 0, NULL, &size, list.data());

        if (err == noErr)
        {
            // iterate through the ranges and add the minimum, maximum, and common rates in between
            UInt32 first = 0;
            UInt32 last  = 0;
            for(UInt32 i = 0; i < size / sizeof(AudioValueRange); i++)
            {
                first = last;
                // find the index of the first common rate greater than or equal to the minimum
                while((first < COMMON_SAMPLE_RATE_COUNT) &&  (CommonSampleRates[first] < list[i].mMinimum))
                    first++;

                if (first >= COMMON_SAMPLE_RATE_COUNT)
                    break;

                last = first;
                // find the index of the first common rate greater than or equal to the maximum
                while((last < COMMON_SAMPLE_RATE_COUNT) && (CommonSampleRates[last] < list[i].mMaximum))
                {
                    results.append(CommonSampleRates[last]);
                    last++;
                }

                if (IsRateCommon(list[i].mMinimum))
                    results.append(list[i].mMinimum);
                else if (IsRateCommon(list[i].mMaximum))
                    results.append(list[i].mMaximum);
            }

            return results;
        }
    }

    LOG(VB_GENERAL, LOG_WARNING, QString("Failed to retrieve sample rates (Error: %1)").arg(err));
    return results;
}

QList<int> AudioOutputOSXPriv::GetChannelsList(AudioDeviceID, bool Passthrough)
{
    QList<int> channels;
    QVector<AudioStreamID> streams = GetStreamsList(m_deviceID);
    if (streams.isEmpty())
        return channels;

    bool founddigital = false;
    if (Passthrough)
    {
        foreach (AudioStreamID stream, streams)
        {
            QVector<AudioStreamBasicDescription> formats = GetFormatsList(stream);
            if (formats.isEmpty())
                continue;

            foreach (AudioStreamBasicDescription format, formats)
            {
                if (format.mFormatID == 'IAC3' || format.mFormatID == kAudioFormat60958AC3)
                {
                    channels.append(format.mChannelsPerFrame);
                    founddigital = true;
                }
            }
        }
    }

    if (!founddigital)
    {
        foreach (AudioStreamID stream, streams)
        {
            QVector<AudioStreamBasicDescription> formats = GetFormatsList(stream);
            if (formats.isEmpty())
                continue;

            foreach (AudioStreamBasicDescription format, formats)
                if (format.mChannelsPerFrame <= CHANNELS_MAX)
                    channels.append(format.mChannelsPerFrame);
        }
    }

    return channels;
}

UInt32 AudioOutputOSXPriv::GetTerminalType(AudioStreamID StreamID)
{
    UInt32 terminaltype;
    UInt32 size = sizeof(UInt32);
    AudioObjectPropertyAddress propertyAddress;
    propertyAddress.mSelector = kAudioStreamPropertyTerminalType;
    propertyAddress.mScope    = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement  = kAudioObjectPropertyElementMaster;

    OSStatus err = AudioObjectGetPropertyData(StreamID, &propertyAddress, 0, NULL, &size, &terminaltype);

    if (err == noErr)
        return terminaltype;

    return OUTPUT_UNDEFINED;
}

UInt32 AudioOutputOSXPriv::GetTransportType(AudioDeviceID DeviceID)
{
    UInt32 transporttype;
    UInt32 size = sizeof(UInt32);
    AudioObjectPropertyAddress propertyAddress;
    propertyAddress.mSelector = kAudioDevicePropertyTransportType;
    propertyAddress.mScope    = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement  = kAudioObjectPropertyElementMaster;

    OSStatus err = AudioObjectGetPropertyData(DeviceID, &propertyAddress, 0, NULL, &size, &transporttype);

    if (err == noErr)
        return transporttype;

    return kAudioDeviceTransportTypeUnknown;
}

QVector<AudioStreamID> AudioOutputOSXPriv::GetStreamsList(AudioDeviceID DeviceID)
{
    UInt32 size = sizeof(UInt32);
    AudioObjectPropertyAddress propertyAddress;
    propertyAddress.mSelector = kAudioDevicePropertyStreams;
    propertyAddress.mScope    = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement  = kAudioObjectPropertyElementMaster;

    OSStatus err = AudioObjectGetPropertyDataSize(DeviceID, &propertyAddress, 0, NULL, &size);

    if (err == noErr)
    {
        QVector<AudioStreamID> list(size / sizeof(AudioStreamID));
        err = AudioObjectGetPropertyData(DeviceID, &propertyAddress, 0, NULL, &size, list.data());

        if (err == noErr)
            return list;
    }

    LOG(VB_GENERAL, LOG_ERR, QString("Failed to retrieve stream list (Error: %1)").arg(err));
    return QVector<AudioStreamID>();
}

QVector<AudioStreamBasicDescription> AudioOutputOSXPriv::GetFormatsList(AudioStreamID StreamID)
{
    UInt32 size = sizeof(UInt32);
    AudioObjectPropertyAddress propertyAddress;
    propertyAddress.mSelector = kAudioStreamPropertyAvailablePhysicalFormats;
    propertyAddress.mScope    = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement  = kAudioObjectPropertyElementMaster;

    OSStatus err = AudioObjectGetPropertyDataSize(StreamID, &propertyAddress, 0, NULL, &size);

    if (err == noErr)
    {
        QVector<AudioStreamBasicDescription> list(size / sizeof(AudioStreamBasicDescription));
        err = AudioObjectGetPropertyData(StreamID, &propertyAddress, 0, NULL, &size, list.data());

        if (err == noErr)
            return list;
    }

    LOG(VB_GENERAL, LOG_ERR, QString("Failed to retrieve formats list (Error: %1)").arg(err));
    return QVector<AudioStreamBasicDescription>();
}

int AudioOutputOSXPriv::ChangeAudioStreamFormat(AudioStreamID StreamID, AudioStreamBasicDescription Format)
{
    AudioObjectPropertyAddress propertyAddress;
    propertyAddress.mSelector = kAudioStreamPropertyPhysicalFormat;
    propertyAddress.mScope    = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement  = kAudioObjectPropertyElementMaster;

    LOG(VB_AUDIO, LOG_INFO, QString("ChangeAudioStreamFormat: %1 -> %2").arg(StreamID).arg(StreamDescriptionToString(Format)));

    OSStatus err = AudioObjectSetPropertyData(StreamID, &propertyAddress, 0, NULL, sizeof(AudioStreamBasicDescription), &Format);

    if (err == noErr)
        return true;

    LOG(VB_GENERAL, LOG_ERR, QString("Failed set stream format (Error: %1)").arg(UInt32ToFourCC(err)));
    return false;
}

OSStatus AudioOutputOSXPriv::DefaultDeviceChangedCallback(AudioObjectID ObjectID, UInt32 NumberAddresses, const AudioObjectPropertyAddress Addresses[], void* Client)
{
    LOG(VB_GENERAL, LOG_INFO, "Default device changed");
    return noErr;
}

OSStatus AudioOutputOSXPriv::DeviceIsAliveCallback(AudioObjectID ObjectID, UInt32 NumberAddresses, const AudioObjectPropertyAddress Addresses[], void *Client)
{
    LOG(VB_GENERAL, LOG_INFO, "Device 'alive' status changed");
    return noErr;
}

OSStatus AudioOutputOSXPriv::DeviceListChangedCallback(AudioObjectID ObjectID, UInt32 NumberAddresses, const AudioObjectPropertyAddress Addresses[], void *Client)
{
    LOG(VB_GENERAL, LOG_INFO, "Device list changed");
    return noErr;
}

/** \class AudioOutputOSX
 *  \brief Implements Core Audio (Mac OS X Hardware Abstraction Layer) output.
*/

AudioOutputOSX::AudioOutputOSX(const AudioSettings &Settings, AudioWrapper *Parent)
  : AudioOutput(Settings, Parent),
    m_bufferedBytes(0)
{
    m_mainDevice.remove(0, 10);
    LOG(VB_AUDIO, LOG_INFO,  QString("AudioOutputOSX: trying to open '%1'").arg(m_mainDevice));
    m_priv = new AudioOutputOSXPriv(this, m_mainDevice);

    InitSettings(Settings);
    if (Settings.m_openOnInit)
        Reconfigure(Settings);
}

AudioOutputOSX::~AudioOutputOSX()
{
    KillAudio();
    delete m_priv;
}

AudioOutputSettings* AudioOutputOSX::GetOutputSettings(bool Digital)
{
    AudioOutputSettings *settings = new AudioOutputSettings();
    QList<int> rates = m_priv->GetSampleRates(m_priv->m_deviceID);

    if (rates.isEmpty())
    {
        settings->AddSupportedRate(48000);
    }
    else
    {
        QList<int> rateslist = settings->GetRates();
        foreach (int rate, rateslist)
            if (rate > 0 && rates.contains(rate))
                settings->AddSupportedRate(rate);
    }

    // Supported format: 16 bits audio or float
    settings->AddSupportedFormat(FORMAT_S16);
    settings->AddSupportedFormat(FORMAT_FLT);

    QList<int> channels = m_priv->GetChannelsList(m_priv->m_deviceID, Digital);
    if (channels.isEmpty())
    {
        settings->AddSupportedChannels(2);
    }
    else
    {
        for (int i = CHANNELS_MIN; i <= CHANNELS_MAX; i++)
        {
            if (channels.contains(i))
            {
                LOG(VB_AUDIO, LOG_DEBUG, QString("Supports %1 channels").arg(i));
                // If 8 channels are supported but not 6, fake 6
                if (i == 8 && !channels.contains(6))
                    settings->AddSupportedChannels(6);
                settings->AddSupportedChannels(i);
            }
        }
    }

    if (m_priv->FindAC3Stream())
        settings->SetPassthrough(PassthroughYes);

    return settings;
}

bool AudioOutputOSX::OpenDevice(void)
{
    if (m_parent)
        m_parent->ClearAudioTime();

    if (m_priv && m_priv->Open(m_passthrough || m_encode))
    {
        if (m_internalVolumeControl && m_setInitialVolume)
        {
            QString controlLabel = gLocalContext->GetSetting("MixerControl", QString("PCM"));
            controlLabel += "MixerVolume";
            SetCurrentVolume(gLocalContext->GetSetting(controlLabel, (int)80));
        }

        return true;
    }

    return false;
}

void AudioOutputOSX::CloseDevice()
{
    if (m_parent)
        m_parent->ClearAudioTime();

    if (m_priv)
        m_priv->Close();
}

bool AudioOutputOSX::RenderAudio(unsigned char *Buffer, int Size, unsigned long long Timestamp)
{
    if (m_pauseAudio || m_killAudio)
    {
        m_actuallyPaused = true;
        return false;
    }

    bool updateparent = true;

    /* This callback is called when the sound system requests
     data.  We don't want to block here, because that would
     just cause dropouts anyway, so we always return whatever
     data is available.  If we haven't received enough, either
     because we've finished playing or we have a buffer
     underrun, we play silence to fill the unused space.  */

    int written_size = GetAudioData(Buffer, Size, false);
    if (written_size && (Size > written_size))
    {
        updateparent = false;
        memset(Buffer + written_size, 0, Size - written_size);
    }

    //Audio received is in SMPTE channel order, reorder to CA unless Passthrough
    if (!m_passthrough && m_channels == 8)
        ReorderSmpteToCA(Buffer, Size / m_outputBytesPerFrame, m_outputFormat);

    // update audiotime (m_bufferedBytes is read by GetBufferedOnSoundcard)
    UInt64 nanos = AudioConvertHostTimeToNanos(Timestamp - AudioGetCurrentHostTime());
    m_bufferedBytes = (int)((nanos / 1000000000.0) *        // secs
                            (m_effectiveDSPRate / 100.0) *  // frames/sec
                             m_outputBytesPerFrame);        // bytes/frame

    if (updateparent && m_parent)
        m_parent->SetAudioTime(GetAudiotime(), TorcCoreUtils::GetMicrosecondCount());

    return (written_size > 0);
}

void AudioOutputOSX::WriteAudio(unsigned char*, int)
{
}

int AudioOutputOSX::GetBufferedOnSoundcard(void) const
{
    return m_bufferedBytes;
}

bool AudioOutputOSX::StartOutputThread(void)
{
    return true;
}

void AudioOutputOSX::StopOutputThread(void)
{
    m_killAudio = true;
}

// Reimplement GetAudiotime() so that we don't use gettimeofday or Qt mutexes.
int64_t AudioOutputOSX::GetAudiotime(void)
{
    int audbuf_timecode = GetBaseAudBufTimeCode();

    if (!audbuf_timecode)
        return 0;

    int totalbuffer = AudioReady() + GetBufferedOnSoundcard();
    return audbuf_timecode - (int)(totalbuffer * 100000.0 / (m_outputBytesPerFrame * m_effectiveDSPRate * m_stretchFactor));
}

int AudioOutputOSX::GetVolumeChannel(int Channel) const
{
    (void)Channel;

    Float32 volume;
    if (!AudioUnitGetParameter(m_priv->m_analogAudioUnit, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, &volume))
        return (int)lroundf(volume * 100.0f);

    return 0;
}

void AudioOutputOSX::SetVolumeChannel(int Channel, int Volume)
{
    (void)Channel;
    if (AudioUnitSetParameter(m_priv->m_analogAudioUnit, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, (Volume * 0.01f), 0) != noErr)
        LOG(VB_GENERAL, LOG_ERR, "Failed to set audio volume");
}

class AudioFactoryOSX : public AudioFactory
{
    void Score(const AudioSettings &Settings, AudioWrapper *Parent, int &Score)
    {
        (void)Parent;
        bool match = Settings.m_mainDevice.startsWith("coreaudio", Qt::CaseInsensitive);
        int score  =  match ? AUDIO_PRIORITY_MATCH : AUDIO_PRIORITY_LOW;
        if (Score <= score)
            Score = score;
    }

    AudioOutput* Create(const AudioSettings &Settings, AudioWrapper *Parent, int &Score)
    {
        bool match = Settings.m_mainDevice.startsWith("coreaudio", Qt::CaseInsensitive);
        int score  =  match ? AUDIO_PRIORITY_MATCH : AUDIO_PRIORITY_LOW;
        if (Score <= score)
            return new AudioOutputOSX(Settings, Parent);
        return NULL;
    }

    void GetDevices(QList<AudioDeviceConfig> &DeviceList)
    {
        QVector<AudioDeviceID> devices = AudioOutputOSXPriv::GetDevices();
        LOG(VB_AUDIO, LOG_INFO,  QString("Number of devices: %1").arg(devices.size()));

        foreach (AudioDeviceID deviceid, devices)
        {
            if (AudioOutputOSXPriv::GetTotalOutputChannels(deviceid) == 0)
                continue;

            AudioDeviceConfig *config = AudioOutput::GetAudioDeviceConfig(QString("CoreAudio:%1").arg(AudioOutputOSXPriv::GetName(deviceid)), QString());
            if (config)
            {
                DeviceList.append(*config);
                delete config;
            }
        }

        AudioDeviceConfig *config = AudioOutput::GetAudioDeviceConfig(QString("CoreAudio:Default"), QString("Default device"));
        if (config)
        {
            DeviceList.append(*config);
            delete config;
        }
    }
} AudioFactoryOSX;
