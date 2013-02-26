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

static UInt32   sNumberCommonSampleRates = 15;
static Float64  sCommonSampleRates[] = {
    8000.0,   11025.0,  12000.0,
    16000.0,  22050.0,  24000.0,
    32000.0,  44100.0,  48000.0,
    64000.0,  88200.0,  96000.0,
    128000.0, 176400.0, 192000.0 };

static bool IsRateCommon(Float64 inRate)
{
    bool theAnswer = false;
    for(UInt32 i = 0; !theAnswer && (i < sNumberCommonSampleRates); i++)
    {
        theAnswer = inRate == sCommonSampleRates[i];
    }
    return theAnswer;
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

class AudioOutputOSXPriv
{
  public:
    AudioOutputOSXPriv(AudioOutputOSX *Parent)
      : m_parent(Parent)
    {
        Initialise();
        ResetAudioDevices();
        m_deviceID = GetDefaultOutputDevice();
    }

    AudioOutputOSXPriv(AudioOutputOSX *Parent, AudioDeviceID DeviceID)
      : m_parent(Parent)
    {
        Initialise();
        ResetAudioDevices();
        m_deviceID = DeviceID;
    }

    AudioOutputOSXPriv(AudioOutputOSX *Parent, QString DeviceName)
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

    void Initialise(void)
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
    }

    AudioDeviceID GetDefaultOutputDevice(void)
    {
        AudioDeviceID deviceId = 0;

        UInt32 paramsize = sizeof(deviceId);
        OSStatus err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice,
                                                &paramsize, &deviceId);
        if (err == noErr)
        {
            LOG(VB_AUDIO, LOG_INFO, QString("Default device ID '%1'").arg(deviceId));
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING, QString("Failed to get default audio device '%1'").arg(UInt32ToFourCC(err)));
            deviceId = 0;
        }

        return deviceId;
    }

    int GetTotalOutputChannels(void)
    {
        UInt32 channels = 0;
        if (!m_deviceID)
            return channels;

        UInt32 size = 0;
        AudioDeviceGetPropertyInfo(m_deviceID, 0, false,
                                   kAudioDevicePropertyStreamConfiguration,
                                   &size, NULL);
        AudioBufferList *list = (AudioBufferList *)malloc(size);
        OSStatus err = AudioDeviceGetProperty(m_deviceID, 0, false,
                                              kAudioDevicePropertyStreamConfiguration,
                                              &size, list);
        if (err == noErr)
        {
            for (UInt32 buffer = 0; buffer < list->mNumberBuffers; buffer++)
                channels += list->mBuffers[buffer].mNumberChannels;
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING, QString(
                 "Unable to get total device output channels - id: %1 Error = [%2]")
                 .arg(m_deviceID).arg(err));
        }

        LOG(VB_AUDIO, LOG_INFO, QString("GetTotalOutputChannels: Found %1 channels in %2 buffers")
              .arg(channels).arg(list->mNumberBuffers));
        free(list);

        return channels;
    }

    QString GetName(void)
    {
        if (!m_deviceID)
            return QString();

        UInt32 propertySize;
        AudioObjectPropertyAddress propertyAddress;

        CFStringRef name;
        propertySize = sizeof(CFStringRef);
        propertyAddress.mSelector = kAudioObjectPropertyName;
        propertyAddress.mScope    = kAudioObjectPropertyScopeGlobal;
        propertyAddress.mElement  = kAudioObjectPropertyElementMaster;
        OSStatus err = AudioObjectGetPropertyData(m_deviceID, &propertyAddress,
                                                  0, NULL, &propertySize, &name);
        if (err != noErr)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("AudioObjectGetPropertyData for kAudioObjectPropertyName error: [%1]").arg(err));
            return QString();
        }

        return CFStringReftoQString(name);
    }

    bool GetAutoHogMode(void)
    {
        UInt32 val = 0;
        UInt32 size = sizeof(val);
        OSStatus err = AudioHardwareGetProperty(kAudioHardwarePropertyHogModeIsAllowed, &size, &val);

        if (err != noErr)
        {
            LOG(VB_GENERAL, LOG_WARNING, QString("Unable to get auto 'hog' mode. Error = [%1]").arg(err));
            return false;
        }

        return (val == 1);
    }

    void SetAutoHogMode(bool Enable)
    {
        UInt32 val = Enable ? 1 : 0;
        OSStatus err = AudioHardwareSetProperty(kAudioHardwarePropertyHogModeIsAllowed,
                                                sizeof(val), &val);
        if (err != noErr)
        {
            LOG(VB_GENERAL, LOG_WARNING, QString("SetAutoHogMode: Unable to set auto 'hog' mode. Error = [%1]")
                 .arg(err));
        }
    }

    pid_t GetHogStatus(void)
    {
        pid_t pid;
        UInt32 pidsize = sizeof(pid);
        OSStatus err = AudioDeviceGetProperty(m_deviceID, 0, FALSE, kAudioDevicePropertyHogMode,
                                              &pidsize, &pid);
        if (err != noErr)
        {
            // This is not a fatal error. Some drivers simply don't support this property
            LOG(VB_AUDIO, LOG_INFO, QString("GetHogStatus: unable to check: [%1]").arg(err));
            return -1;
        }

        return pid;
    }

    bool SetHogStatus(bool Hog)
    {
        // According to Jeff Moore (Core Audio, Apple), Setting kAudioDevicePropertyHogMode
        // is a toggle and the only way to tell if you do get hog mode is to compare
        // the returned pid against getpid, if the match, you have hog mode, if not you don't.
        if (!m_deviceID)
            return false;

        if (Hog)
        {
            if (m_hog == -1)
            {
                LOG(VB_AUDIO, LOG_INFO, QString("Setting 'hog' status on device %1").arg(m_deviceID));
                OSStatus err = AudioDeviceSetProperty(m_deviceID, NULL, 0, false,
                                                      kAudioDevicePropertyHogMode,
                                                      sizeof(m_hog), &m_hog);
                if (err || m_hog != getpid())
                {
                    LOG(VB_GENERAL, LOG_WARNING, QString("Unable to set 'hog' status. Error = [%1]")
                         .arg(UInt32ToFourCC(err)));
                    return false;
                }
                LOG(VB_AUDIO, LOG_INFO, QString("Successfully set 'hog' status on device %1")
                      .arg(m_deviceID));
            }
        }
        else
        {
            if (m_hog > -1)
            {
                LOG(VB_AUDIO, LOG_INFO, QString("Releasing 'hog' status on device %1").arg(m_deviceID));
                pid_t hogPid = -1;
                OSStatus err = AudioDeviceSetProperty(m_deviceID, NULL, 0, false,
                                                      kAudioDevicePropertyHogMode,
                                                      sizeof(hogPid), &hogPid);
                if (err || hogPid == getpid())
                {
                    LOG(VB_GENERAL, LOG_WARNING, QString("Unable to release 'hog' status. Error = [%1]").arg(UInt32ToFourCC(err)));
                    return false;
                }

                m_hog = hogPid;
            }
        }

        return true;
    }


    AudioDeviceID GetDeviceWithName(QString DeviceName)
    {
        UInt32 size = 0;
        AudioDeviceID deviceID = 0;

        AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices, &size, NULL);
        UInt32 deviceCount = size / sizeof(AudioDeviceID);
        AudioDeviceID* devices = new AudioDeviceID[deviceCount];

        OSStatus err = AudioHardwareGetProperty(kAudioHardwarePropertyDevices, &size, devices);
        if (err != noErr)
        {
            LOG(VB_GENERAL, LOG_WARNING, QString("Unable to retrieve the list of available devices. "
                         "Error [%1]").arg(err));
        }
        else
        {
            for (UInt32 dev = 0; dev < deviceCount; dev++)
            {
                AudioOutputOSXPriv device(NULL, devices[dev]);
                if (device.GetTotalOutputChannels() == 0)
                    continue;

                QString name = device.GetName();
                if (!name.isEmpty() && name == DeviceName)
                {
                    LOG(VB_AUDIO, LOG_INFO, QString("Found: %1").arg(name));
                    deviceID = devices[dev];
                }

                if (deviceID)
                    break;
            }
        }

        delete [] devices;
        return deviceID;
    }

    int OpenAnalog(void)
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

    static OSStatus RenderCallbackAnalog(void                       *inRefCon,
                                         AudioUnitRenderActionFlags *ioActionFlags,
                                         const AudioTimeStamp       *inTimeStamp,
                                         UInt32                     inBusNumber,
                                         UInt32                     inNumberFrames,
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

    void CloseAnalog(void)
    {
        if (m_analogAudioUnit)
        {
            OSStatus err;

            if (m_started)
            {
                err = AudioOutputUnitStop(m_analogAudioUnit);
                LOG(VB_AUDIO, LOG_INFO, QString("AudioOutputUnitStop %1").arg(err));
            }

            if (m_initialised)
            {
                err = AudioUnitUninitialize(m_analogAudioUnit);
                LOG(VB_AUDIO, LOG_INFO, QString("AudioUnitUninitialize %1").arg(err));
            }

            err = CloseComponent(m_analogAudioUnit);
            LOG(VB_AUDIO, LOG_INFO, QString("CloseComponent %1").arg(err));

            m_analogAudioUnit = NULL;
        }

        m_hasCallback      = false;
        m_initialised = false;
        m_started     = false;
        m_wasDigital  = false;
    }

    bool OpenSPDIF(void)
    {
        OSStatus       err;
        AudioStreamBasicDescription outputFormat = {0};

        LOG(VB_AUDIO, LOG_INFO, "OpenSPDIF");

        QList<AudioStreamID> streams = StreamsList(m_deviceID);
        if (streams.isEmpty())
        {
            LOG(VB_GENERAL, LOG_WARNING, "Couldn't retrieve list of streams");
            return false;
        }

        for (int i = 0; i < streams.size(); ++i)
        {
            LOG(VB_GENERAL, LOG_INFO, QString::number(i));
            QList<AudioStreamBasicDescription> formats = FormatsList(streams[i]);
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
            UInt32 paramsize = sizeof(m_formatOrig);
            err = AudioStreamGetProperty(m_streamID, 0, kAudioStreamPropertyPhysicalFormat,
                                         &paramsize, &m_formatOrig);
            if (err != noErr)
            {
                LOG(VB_GENERAL, LOG_WARNING, QString("OpenSPDIF - could not retrieve the original streamformat: [%1]")
                     .arg(UInt32ToFourCC(err)));
            }
            else
            {
                m_revertFormat = true;
            }
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
        if (!AudioStreamChangeFormat(m_streamID, m_formatNew))
            return false;

        m_bytesPerPacket = m_formatNew.mBytesPerPacket;
        err = AudioDeviceAddIOProc(m_deviceID, (AudioDeviceIOProc)RenderCallbackSPDIF, (void *)this);

        if (err != noErr)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("AudioDeviceAddIOProc failed: [%1]")
                  .arg(UInt32ToFourCC(err)));
            return false;
        }

        m_hasCallback = true;
        err = AudioDeviceStart(m_deviceID, (AudioDeviceIOProc)RenderCallbackSPDIF);

        if (err != noErr)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("AudioDeviceStart failed: [%1]")
                  .arg(UInt32ToFourCC(err)));
            return false;
        }

        m_started = true;
        return true;
    }

    static OSStatus RenderCallbackSPDIF(AudioDeviceID         inDevice,
                                        const AudioTimeStamp *inNow,
                                        const void           *inInputData,
                                        const AudioTimeStamp *inInputTime,
                                        AudioBufferList      *outOutputData,
                                        const AudioTimeStamp *inOutputTime,
                                        void                 *inRefCon)
    {
        AudioOutputOSXPriv    *d = static_cast<AudioOutputOSXPriv *>(inRefCon);
        AudioOutputOSX    *a = d->m_parent;
        int           index = d->m_streamIndex;

        (void)inDevice;
        (void)inNow;
        (void)inInputData;
        (void)inInputTime;

        /*
         * HACK: No idea why this would be the case, but after the second run, we get
         * incorrect value
         */
        if (d->m_bytesPerPacket > 0 && outOutputData->mBuffers[index].mDataByteSize > d->m_bytesPerPacket)
            outOutputData->mBuffers[index].mDataByteSize = d->m_bytesPerPacket;

        if (!a->RenderAudio((unsigned char *)(outOutputData->mBuffers[index].mData),
                            outOutputData->mBuffers[index].mDataByteSize, inOutputTime->mHostTime))
        {
            memset(outOutputData->mBuffers[index].mData, 0, outOutputData->mBuffers[index].mDataByteSize);
        }

        return noErr;
    }

    void CloseSPDIF(void)
    {
        if (!m_digitalInUse)
            return;

        if (m_started)
        {
            OSStatus err = AudioDeviceStop(m_deviceID, (AudioDeviceIOProc)RenderCallbackSPDIF);
            if (err != noErr)
            {
                LOG(VB_GENERAL, LOG_ERR, QString("AudioDeviceStop failed: [%1]")
                      .arg(UInt32ToFourCC(err)));
            }

            m_started = false;
        }

        if (m_hasCallback)
        {
            OSStatus err = AudioDeviceRemoveIOProc(m_deviceID, (AudioDeviceIOProc)RenderCallbackSPDIF);
            if (err != noErr)
            {
                LOG(VB_GENERAL, LOG_ERR, QString("AudioDeviceRemoveIOProc failed: [%1]")
                      .arg(UInt32ToFourCC(err)));
            }

            m_hasCallback = false;
        }

        if (m_revertFormat)
        {
            AudioStreamChangeFormat(m_streamID, m_formatOrig);
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

    bool SetMixingSupport(bool Mix)
    {
        if (!m_deviceID)
            return false;

        int restore = -1;
        if (m_restoreMixer == -1) // This is our first change to this setting. Store the original setting for restore
            restore = (GetMixingSupport() ? 1 : 0);

        UInt32 mixEnable = Mix ? 1 : 0;
        LOG(VB_AUDIO, LOG_INFO, QString("%1abling mixing for device %2").arg(Mix ? "En" : "Dis").arg(m_deviceID));

        OSStatus err = AudioDeviceSetProperty(m_deviceID, NULL, 0, false,
                                              kAudioDevicePropertySupportsMixing,
                                              sizeof(mixEnable), &mixEnable);
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

    bool GetMixingSupport(void)
    {
        if (!m_deviceID)
            return false;

        UInt32 val = 0;
        UInt32 size = sizeof(val);
        OSStatus err = AudioDeviceGetProperty(m_deviceID, 0, false,
                                              kAudioDevicePropertySupportsMixing,
                                              &size, &val);
        if (err != noErr)
            return false;

        return (val > 0);
    }

    bool FindAC3Stream(void)
    {
        bool found = false;

        QList<AudioStreamID> streams = StreamsList(m_deviceID);
        if (streams.isEmpty())
            return found;

        foreach (AudioStreamID stream, streams)
        {
            QList<AudioStreamBasicDescription> formats = FormatsList(stream);
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

    void ResetAudioDevices(void)
    {
        UInt32 size;
        AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices, &size, NULL);
        AudioDeviceID  *devices = (AudioDeviceID*)malloc(size);

        if (!devices)
            return;

        int numDevices = size / sizeof(AudioDeviceID);
        AudioHardwareGetProperty(kAudioHardwarePropertyDevices, &size, devices);

        for (int i = 0; i < numDevices; i++)
        {
            QList<AudioStreamID> streams = StreamsList(devices[i]);
            if (streams.isEmpty())
                continue;

            foreach (AudioStreamID stream, streams)
                ResetStream(stream);
        }

        free(devices);
    }

    void ResetStream(AudioStreamID StreamID)
    {
        AudioStreamBasicDescription  currentformat;
        OSStatus                     err;
        UInt32 paramsize = sizeof(currentformat);
        AudioStreamGetProperty(StreamID, 0, kAudioStreamPropertyPhysicalFormat,
                               &paramsize, &currentformat);

        // If it's currently AC-3/SPDIF then reset it to some mixable format
        if (currentformat.mFormatID == 'IAC3' || currentformat.mFormatID == kAudioFormat60958AC3)
        {
            QList<AudioStreamBasicDescription> formats = FormatsList(StreamID);
            bool streamreset = false;
            if (formats.isEmpty())
                return;

            foreach (AudioStreamBasicDescription format, formats)
            {
                if (!streamreset && format.mFormatID == kAudioFormatLinearPCM)
                {
                    err = AudioStreamSetProperty(StreamID, NULL, 0,
                                                 kAudioStreamPropertyPhysicalFormat,
                                                 sizeof(format), &(format));
                    if (err != noErr)
                    {
                        LOG(VB_GENERAL, LOG_WARNING, QString("ResetStream: could not set physical format: [%1]")
                             .arg(UInt32ToFourCC(err)));
                        continue;
                    }
                    else
                    {
                        streamreset = true;
                        sleep(1);   // For the change to take effect
                    }
                }
            }
        }
    }

    QList<int> RatesList(AudioDeviceID DeviceID)
    {
        QList<int> results;

        UInt32 listsize;
        OSStatus err = AudioDeviceGetPropertyInfo(DeviceID, 0, 0, kAudioDevicePropertyAvailableNominalSampleRates,
                                                  &listsize, NULL);
        if (err != noErr)
        {
            LOG(VB_GENERAL, LOG_WARNING, QString("Couldn't get data rate list size: [%1]")
                 .arg(err));
            return results;
        }

        AudioValueRange *list = (AudioValueRange*)malloc(listsize);
        if (list == NULL)
            return results;

        err = AudioDeviceGetProperty(DeviceID, 0, 0, kAudioDevicePropertyAvailableNominalSampleRates,
                                     &listsize, list);
        if (err != noErr)
        {
            LOG(VB_GENERAL, LOG_WARNING, QString("couldn't get list: [%1]")
                 .arg(err));
            return results;
        }

        // iterate through the ranges and add the minimum, maximum, and common rates in between
        UInt32 theFirstIndex = 0, theLastIndex = 0;
        for(UInt32 i = 0; i < listsize / sizeof(AudioValueRange); i++)
        {
            theFirstIndex = theLastIndex;
            // find the index of the first common rate greater than or equal to the minimum
            while((theFirstIndex < sNumberCommonSampleRates) &&  (sCommonSampleRates[theFirstIndex] < list[i].mMinimum))
                theFirstIndex++;

            if (theFirstIndex >= sNumberCommonSampleRates)
                break;

            theLastIndex = theFirstIndex;
            // find the index of the first common rate greater than or equal to the maximum
            while((theLastIndex < sNumberCommonSampleRates) && (sCommonSampleRates[theLastIndex] < list[i].mMaximum))
            {
                results.append(sCommonSampleRates[theLastIndex]);
                theLastIndex++;
            }

            if (IsRateCommon(list[i].mMinimum))
                results.append(list[i].mMinimum);
            else if (IsRateCommon(list[i].mMaximum))
                results.append(list[i].mMaximum);
        }

        free(list);
        return results;
    }

    QList<int> ChannelsList(AudioDeviceID DeviceID, bool Passthrough)
    {
        (void)DeviceID;

        QList<int> channels;
        QList<AudioStreamID> streams = StreamsList(m_deviceID);
        if (streams.isEmpty())
            return channels;

        bool founddigital = false;
        if (Passthrough)
        {
            foreach (AudioStreamID stream, streams)
            {
                QList<AudioStreamBasicDescription> formats = FormatsList(stream);
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
                QList<AudioStreamBasicDescription> formats = FormatsList(stream);
                if (formats.isEmpty())
                    continue;

                foreach (AudioStreamBasicDescription format, formats)
                    if (format.mChannelsPerFrame <= CHANNELS_MAX)
                        channels.append(format.mChannelsPerFrame);
            }
        }

        return channels;
    }

    QList<AudioStreamID> StreamsList(AudioDeviceID DeviceID)
    {
        QList<AudioStreamID> streams;

        UInt32 listsize;
        OSStatus err = AudioDeviceGetPropertyInfo(DeviceID, 0, FALSE, kAudioDevicePropertyStreams,
                                                  &listsize, NULL);
        if (err != noErr)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Could not get list size: [%1]")
                  .arg(UInt32ToFourCC(err)));
            return streams;
        }

        AudioStreamID *list = (AudioStreamID*)malloc(listsize);
        if (list == NULL)
            return streams;

        err = AudioDeviceGetProperty(DeviceID, 0, FALSE, kAudioDevicePropertyStreams,
                                     &listsize, list);
        if (err != noErr)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Could not get list: [%1]")
                  .arg(UInt32ToFourCC(err)));
            free(list);
            return streams;
        }

        uint count = listsize / sizeof(AudioStreamID);
        for (uint i = 0; i < count; ++i)
            streams.append(list[i]);

        free(list);
        return streams;
    }

    QList<AudioStreamBasicDescription> FormatsList(AudioStreamID StreamID)
    {
        QList<AudioStreamBasicDescription> formats;

        UInt32 listsize;
        AudioDevicePropertyID property = kAudioStreamPropertyAvailablePhysicalFormats;
        OSStatus err = AudioStreamGetPropertyInfo(StreamID, 0, property, &listsize, NULL);
        if (err != noErr)
        {
            LOG(VB_GENERAL, LOG_WARNING, QString("Couldn't get list size: [%1] id %2")
                .arg(UInt32ToFourCC(err)).arg(StreamID));
            return formats;
        }

        AudioStreamBasicDescription *list = (AudioStreamBasicDescription *)malloc(listsize);
        if (list == NULL)
            return formats;

        err = AudioStreamGetProperty(StreamID, 0, kAudioStreamPropertyPhysicalFormats, &listsize, list);
        if (err != noErr)
        {
            LOG(VB_GENERAL, LOG_WARNING, QString("FormatsList: couldn't get list: [%1]")
                 .arg(UInt32ToFourCC(err)));
            free(list);
            return formats;
        }

        uint count = listsize / sizeof(AudioStreamBasicDescription);
        for (uint i = 0; i < count; ++i)
            formats.append(list[i]);

        free(list);
        return formats;
    }

    int AudioStreamChangeFormat(AudioStreamID               StreamID,
                                AudioStreamBasicDescription Format)
    {
        LOG(VB_AUDIO, LOG_INFO, QString("AudioStreamChangeFormat: %1 -> %2")
              .arg(StreamID).arg(StreamDescriptionToString(Format)));

        OSStatus err = AudioStreamSetProperty(StreamID, 0, 0,
                                              kAudioStreamPropertyPhysicalFormat,
                                              sizeof(Format), &Format);
        if (err != noErr)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("AudioStreamChangeFormat couldn't set stream format: [%1]").arg(UInt32ToFourCC(err)));
            return false;
        }

        return true;
    }

  public:
    AudioOutputOSX  *m_parent;

    // Analog
    AudioUnit      m_analogAudioUnit;

    // Digital
    bool           m_digitalInUse;
    pid_t          m_hog;
    int            m_restoreMixer;
    AudioDeviceID  m_deviceID;
    AudioStreamID  m_streamID;     // StreamID that has a cac3 streamformat
    int            m_streamIndex;  // Index of m_streamID in an AudioBufferList
    UInt32         m_bytesPerPacket;
    AudioStreamBasicDescription m_formatOrig;
    AudioStreamBasicDescription m_formatNew;
    bool           m_revertFormat; // Do we need to revert the stream format?
    bool           m_hasCallback;
    bool           m_initialised;
    bool           m_started;
    bool           m_wasDigital;
};

/** \class AudioOutputOSX
 *  \brief Implements Core Audio (Mac OS X Hardware Abstraction Layer) output.
 */

AudioOutputOSX::AudioOutputOSX(const AudioSettings &Settings)
  : AudioOutput(Settings),
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
    QList<int> rates = m_priv->RatesList(m_priv->m_deviceID);

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

    QList<int> channels = m_priv->ChannelsList(m_priv->m_deviceID, Digital);
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
    bool deviceOpened = false;

    if (m_passthrough || m_encode)
    {
        LOG(VB_AUDIO, LOG_DEBUG, "Trying Digital output");
        if (!(deviceOpened = m_priv->OpenSPDIF()))
            m_priv->CloseSPDIF();
    }

    if (!deviceOpened)
    {
        LOG(VB_AUDIO, LOG_DEBUG, "Trying Analog output");
        int result = -1;

        //for (int i=0; result < 0 && i < 10; i++)
        {
            result = m_priv->OpenAnalog();
            LOG(VB_AUDIO, LOG_DEBUG, QString("OpenAnalog '%1'").arg(result));

            if (result < 0)
            {
                m_priv->CloseAnalog();
                TorcUSleep(1000 * 1000);
            }
        }

        deviceOpened = (result > 0);
    }

    if (!deviceOpened)
    {
        LOG(VB_GENERAL, LOG_ERR, "Couldn't open audio device");
        m_priv->CloseAnalog();
        return false;
    }

    if (m_internalVolumeControl && m_setInitialVolume)
    {
        QString controlLabel = gLocalContext->GetSetting("MixerControl", QString("PCM"));
        controlLabel += "MixerVolume";
        SetCurrentVolume(gLocalContext->GetSetting(controlLabel, (int)80));
    }

    return true;
}

void AudioOutputOSX::CloseDevice()
{
    LOG(VB_AUDIO, LOG_INFO,  QString("Closing %1").arg(m_priv->m_digitalInUse ? "SPDIF" : "Analog"));

    if (m_priv->m_digitalInUse)
        m_priv->CloseSPDIF();
    else
        m_priv->CloseAnalog();
}

bool AudioOutputOSX::RenderAudio(unsigned char *Buffer, int Size, unsigned long long Timestamp)
{
    if (m_pauseAudio || m_killAudio)
    {
        m_actuallyPaused = true;
        return false;
    }

    /* This callback is called when the sound system requests
     data.  We don't want to block here, because that would
     just cause dropouts anyway, so we always return whatever
     data is available.  If we haven't received enough, either
     because we've finished playing or we have a buffer
     underrun, we play silence to fill the unused space.  */

    int written_size = GetAudioData(Buffer, Size, false);
    if (written_size && (Size > written_size))
    {
        // play silence on buffer underrun
        memset(Buffer + written_size, 0, Size - written_size);
    }

    //Audio received is in SMPTE channel order, reorder to CA unless Passthrough
    if (!m_passthrough && m_channels == 8)
    {
        ReorderSmpteToCA(Buffer, Size / m_outputBytesPerFrame, m_outputFormat);
    }

    // update audiotime (m_bufferedBytes is read by GetBufferedOnSoundcard)
    UInt64 nanos = AudioConvertHostTimeToNanos(Timestamp - AudioGetCurrentHostTime());
    m_bufferedBytes = (int)((nanos / 1000000000.0) * // secs
                          (m_effectiveDSPRate / 100.0) *         // frames/sec
                          m_outputBytesPerFrame);   // bytes/frame

    return (written_size > 0);
}

void AudioOutputOSX::WriteAudio(unsigned char *Buffer, int Size)
{
    (void)Buffer;
    (void)Size;
    return;
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
    // FIXME: this only returns global volume
    (void)Channel;

    Float32 volume;
    if (!AudioUnitGetParameter(m_priv->m_analogAudioUnit,
                               kHALOutputParam_Volume,
                               kAudioUnitScope_Global, 0, &volume))
    {
        return (int)lroundf(volume * 100.0f);
    }

    return 0;
}

void AudioOutputOSX::SetVolumeChannel(int Channel, int Volume)
{
    // FIXME: this only sets global volume
    (void)Channel;
    AudioUnitSetParameter(m_priv->m_analogAudioUnit, kHALOutputParam_Volume,
                          kAudioUnitScope_Global, 0, (Volume * 0.01f), 0);
}

class AudioFactoryOSX : public AudioFactory
{
    void Score(const AudioSettings &Settings, int &Score)
    {
        bool match = Settings.m_mainDevice.startsWith("coreaudio", Qt::CaseInsensitive);
        int score  =  match ? AUDIO_PRIORITY_MATCH : AUDIO_PRIORITY_LOW;
        if (Score <= score)
            Score = score;
    }

    AudioOutput* Create(const AudioSettings &Settings, int &Score)
    {
        bool match = Settings.m_mainDevice.startsWith("coreaudio", Qt::CaseInsensitive);
        int score  =  match ? AUDIO_PRIORITY_MATCH : AUDIO_PRIORITY_LOW;
        if (Score <= score)
            return new AudioOutputOSX(Settings);
        return NULL;
    }

    void GetDevices(QList<AudioDeviceConfig> &DeviceList)
    {
        UInt32 size = 0;
        AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices, &size, NULL);
        UInt32 deviceCount = size / sizeof(AudioDeviceID);
        AudioDeviceID* devices = new AudioDeviceID[deviceCount];

        OSStatus error = AudioHardwareGetProperty(kAudioHardwarePropertyDevices, &size, devices);
        if (error)
        {
            LOG(VB_AUDIO, LOG_INFO, QString("Unable to retrieve the list of available devices. Error [%1]").arg(error));
        }
        else
        {
            LOG(VB_AUDIO, LOG_INFO,  QString("GetDevices: Number of devices: %1").arg(deviceCount));

            for (UInt32 dev = 0; dev < deviceCount; dev++)
            {
                AudioOutputOSXPriv device(NULL, devices[dev]);
                if (device.GetTotalOutputChannels() == 0)
                    continue;

                AudioDeviceConfig *config = AudioOutput::GetAudioDeviceConfig(QString("CoreAudio:%1").arg(device.GetName()), QString());
                if (config)
                {
                    DeviceList.append(*config);
                    delete config;
                }
            }
        }

        delete [] devices;

        AudioDeviceConfig *config = AudioOutput::GetAudioDeviceConfig(QString("CoreAudio:Default"), QString("Default device"));
        if (config)
        {
            DeviceList.append(*config);
            delete config;
        }
    }
} AudioFactoryOSX;
