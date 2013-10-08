#ifndef AUDIOOUTPUTWIN_H
#define AUDIOOUTPUTWIN_H

// Torc
#include "../audiooutput.h"

class AudioOutputWinPrivate;

class AudioOutputWin : public AudioOutput
{
    friend class AudioOutputWinPrivate;

  public:
    AudioOutputWin(const AudioSettings &Settings, AudioWrapper *Parent);
    virtual ~AudioOutputWin();

    int                    GetVolumeChannel       (int Channel) const;
    void                   SetVolumeChannel       (int Channel, int Volume);

  protected:
    bool                   OpenDevice             (void);
    void                   CloseDevice            (void);
    void                   WriteAudio             (unsigned char *Buffer, int Size);
    int                    GetBufferedOnSoundcard (void) const;
    AudioOutputSettings*   GetOutputSettings      (bool Passthrough);

  protected:
    AudioOutputWinPrivate *m_priv;
    long                   m_packetCount;
    uint                   m_currentPacket;
    unsigned char        **m_packetBuffer;
    bool                   m_useSPDIF;
    static const uint      kPacketCount;
};

#endif // AUDIOOUTPUTWIN_H
