#ifndef AUDIOOUTPUTNULL_H
#define AUDIOOUTPUTNULL_H

// Torc
#include "audiooutput.h"

#define NULL_BUFFER_SIZE 32768

class AudioOutputNULL : public AudioOutput
{
  public:
    AudioOutputNULL(const AudioSettings &Settings);
    virtual ~AudioOutputNULL();

    void Reset                  (void);
    int  GetVolumeChannel       (int Channel) const;
    void SetVolumeChannel       (int Channel, int Volume);
    int  ReadOutputData         (unsigned char *ReadBuffer, int MaxLength);

  protected:
    AudioOutputSettings*
         GetOutputSettings      (bool Digital);
    bool OpenDevice             (void);
    void CloseDevice            (void);
    void WriteAudio             (unsigned char *Buffer, int Size);
    int  GetBufferedOnSoundcard (void) const;

  private:
    unsigned char m_pcmBuffer[NULL_BUFFER_SIZE];
    QMutex        m_pcmBufferLock;
    int           m_currentBufferSize;
};

#endif

