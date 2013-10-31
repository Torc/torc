#ifndef TORCQMLDISPLAYX11_H
#define TORCQMLDISPLAYX11_H

// Torc
#include "torcqmldisplay.h"

class TorcQMLDisplayX11 : public TorcQMLDisplay
{
  public:
    TorcQMLDisplayX11(QWindow *Window);
    virtual ~TorcQMLDisplayX11();

    virtual void    UpdateScreenData              (void);
    virtual void    RefreshScreenModes            (void);
};

#endif // TORCQMLDISPLAYX11_H
