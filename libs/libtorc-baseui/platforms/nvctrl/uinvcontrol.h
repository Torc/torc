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
    static bool       NVControlAvailable   (Display *XDisplay);
    static QByteArray GetNVEDID            (Display *XDisplay, int Screen);
    static void       InitialiseMetaModes  (Display *XDisplay, int Screen);
};

#endif // UINVCONTROL_H
