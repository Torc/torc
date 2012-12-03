#ifndef UINVCONTROL_H
#define UINVCONTROL_H

// Qt
#include <QMutex>
#include <QByteArray>

// X11
extern "C" {
#include <X11/Xlib.h>
}

class UINVControl
{
  public:
    static bool       NVControlAvailable (void);
    static QByteArray GetNVEDID          (Display *XDisplay, int Screen);
};

#endif // UINVCONTROL_H
