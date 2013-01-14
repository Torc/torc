#ifndef TORCLOCALDEFS_H
#define TORCLOCALDEFS_H

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif

#define TORC_MAIN_THREAD  QString("MainLoop")
#define TORC_ADMIN_THREAD QString("AdminLoop")

#define TORC_KEYEVENT_MODIFIERS           (Qt::NoModifier | Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier | Qt::KeypadModifier)
#define TORC_KEYEVENT_PRINTABLE_MODIFIERS (Qt::NoModifier | Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)

#define TORC_CORE                  QString("CORE_")
#define TORC_GUI                   QString("GUI_")

#define TORC_GUI_TENFOOT_THEMES    QString("tenfootthemes")
#define TORC_GUI_TENINCH_THEMES    QString("teninchthemes")
#define TORC_GUI_DESKTOP_THEMES    QString("desktopthemes")

#endif // TORCLOCALDEFS_H
