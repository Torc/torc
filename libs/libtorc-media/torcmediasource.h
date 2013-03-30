#ifndef TORCMEDIASOURCE_H
#define TORCMEDIASOURCE_H

// Qt
#include <QMutex>

// Torc
#include "torcmediaexport.h"

class TORC_MEDIA_PUBLIC TorcMediaSource
{
  public:
    static void      CreateSources    (void);
    static void      DestroySources   (void);

  public:
    TorcMediaSource(void);
    virtual ~TorcMediaSource();

    TorcMediaSource* GetNextSource    (void);
    virtual void     Create           (void) = 0;
    virtual void     Destroy          (void) = 0;

  private:
    static TorcMediaSource           *gTorcMediaSource;
    TorcMediaSource                  *m_nextTorcMediaSource;
};

#endif // TORCMEDIASOURCE_H
