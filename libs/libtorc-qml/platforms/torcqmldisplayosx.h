#ifndef TORCQMLDISPLAYOSX_H
#define TORCQMLDISPLAYOSX_H

// Torc
#include "torcqmldisplay.h"

class TorcQMLDisplayOSX : public TorcQMLDisplay
{
  public:
    TorcQMLDisplayOSX(QWindow *Window);
    virtual ~TorcQMLDisplayOSX();

  protected:
    virtual void     UpdateScreenData               (void);
    virtual void     RefreshScreenModes             (void);

  protected:
    CFArrayRef       m_displayModes;
};

#endif // TORCQMLDISPLAYOSX_H
