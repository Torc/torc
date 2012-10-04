#ifndef AUDIOOUTPUTLISTENERS_H
#define AUDIOOUTPUTLISTENERS_H

// Qt
#include <QList>

// Torc
#include "torcaudioexport.h"

class TORC_AUDIO_PUBLIC AudioOutputListener
{
  public:
    AudioOutputListener();
    virtual ~AudioOutputListener();
    virtual void AddSample (unsigned char *Buffer, unsigned long Length,
                            unsigned long  Written, int Channels, int Precision) = 0;
    virtual void Prepare   (void) = 0;
    QMutex*      GetLock   (void);

  private:
    QMutex *m_lock;
};

class TORC_AUDIO_PUBLIC AudioOutputListeners
{
  public:
    AudioOutputListeners();
    virtual ~AudioOutputListeners();

    bool         HasListeners     (void);
    void         AddListener      (AudioOutputListener *Listener);
    void         RemoveListener   (AudioOutputListener *Listener);
    QMutex*      GetLock          (void);
    void         SetBufferSize    (unsigned int Size);
    unsigned int GetBufferSize    (void) const;

  protected:
    void         UpdateListeners  (unsigned char *Buffer,  unsigned long Length,
                                   unsigned long  Written, int Channels, int Precision);
    void         PrepareListeners (void);

  private:
    class QMutex                  *m_lock;
    QList<AudioOutputListener*>    m_listeners;
    uint                           m_bufferSize;
};

#endif
