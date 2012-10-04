/* Class TorcFileBuffer
*
* This file is part of the Torc project.
*
* Based on the class VolumeBase from the MythTV project.
* Copyright various authors.
*
* Adapted for Torc by Mark Kendall (2012)
*/

// Qt
#include <QString>

// Torc
#include "torclocalcontext.h"
#include "audiovolume.h"

/*! \class AudioVolume
 *  \brief The base implementation of volume control.
*/

AudioVolume::AudioVolume()
  : m_volume(80),
    m_currentMuteState(kMuteOff)
{
    m_internalVolumeControl = gLocalContext->GetSetting(TORC_CORE + "InternalVolumeControl", 1);
    QString mixerdevice     = gLocalContext->GetSetting(TORC_CORE + "MixerDevice", QString("default"));
    m_softwareVolume        = m_softwareVolumeSetting = (mixerdevice.toLower() == "software");
}

AudioVolume::~AudioVolume()
{
}

bool AudioVolume::SWVolume(void) const
{
    return m_softwareVolume;
}

void AudioVolume::SWVolume(bool Set)
{
    if (m_softwareVolumeSetting)
        return;
    m_softwareVolume = Set;
}

uint AudioVolume::GetCurrentVolume(void) const
{
    return m_volume;
}

void AudioVolume::SetCurrentVolume(int Volume)
{
    m_volume = std::max(std::min(Volume, 100), 0);
    UpdateVolume();

    QString controlLabel = gLocalContext->GetSetting("MixerControl", QString("PCM"));
    controlLabel += "MixerVolume";
    gLocalContext->SetSetting(controlLabel, m_volume);
}

void AudioVolume::AdjustCurrentVolume(int Change)
{
    SetCurrentVolume(m_volume + Change);
}

MuteState AudioVolume::SetMuteState(MuteState State)
{
    m_currentMuteState = State;
    UpdateVolume();
    return m_currentMuteState;
}

void AudioVolume::ToggleMute(void)
{
    bool is_muted = GetMuteState() == kMuteAll;
    SetMuteState((is_muted) ? kMuteOff : kMuteAll);
}

MuteState AudioVolume::GetMuteState(void) const
{
    return m_currentMuteState;
}

MuteState AudioVolume::NextMuteState(MuteState State)
{
    MuteState next = State;

    switch (State)
    {
       case kMuteOff:
           next = kMuteLeft;
           break;
       case kMuteLeft:
           next = kMuteRight;
           break;
       case kMuteRight:
           next = kMuteAll;
           break;
       case kMuteAll:
           next = kMuteOff;
           break;
    }

    return next;
}

void AudioVolume::UpdateVolume(void)
{
    int newvolume = m_volume;
    bool save = true;

    if (m_currentMuteState == kMuteAll)
    {
        newvolume = 0;
        save = false;
    }

    if (m_softwareVolume)
    {
        SetSWVolume(newvolume, save);
        return;
    }
    
    for (int i = 0; i < m_channels; i++)
        SetVolumeChannel(i, newvolume);
    }

void AudioVolume::SyncVolume(void)
{
    // Read the volume from the audio driver and setup our internal state to match
    m_volume = m_softwareVolume ? GetSWVolume() : GetVolumeChannel(0);
}

void AudioVolume::SetChannels(int Channels)
{
    m_channels = Channels;
}
