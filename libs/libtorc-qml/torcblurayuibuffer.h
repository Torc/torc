#ifndef TORCBLURAYUIBUFFER_H
#define TORCBLURAYUIBUFFER_H

// Torc
#include "torcqmlexport.h"
#include "torcbluraybuffer.h"

class TorcBlurayUIHandler;

class TORC_QML_PUBLIC TorcBlurayUIBuffer : public TorcBlurayBuffer
{
  public:
    TorcBlurayUIBuffer(void *Parent, const QString &URI, int *Abort);
    ~TorcBlurayUIBuffer();

    void         InitialiseAVContext  (void* Context);
    void*        RequiredAVFormat     (bool &BufferRequired);
    bool         Open                 (void);
    bool         HandleEvent          (QEvent *Event);

  protected:
    TorcBlurayUIHandler *m_uiHandler;
};

#endif // TORCBLURAYUIBUFFER_H
