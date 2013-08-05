#ifndef TORCLOCALDEFS_H
#define TORCLOCALDEFS_H

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0502 // XP SP 2
#endif

#define TORC_MAIN_THREAD  QString("MainLoop")
#define TORC_ADMIN_THREAD QString("AdminLoop")

#define TORC_KEYEVENT_MODIFIERS           (Qt::NoModifier | Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier | Qt::KeypadModifier)
#define TORC_KEYEVENT_PRINTABLE_MODIFIERS (Qt::NoModifier | Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)

#define TORC_CORE                  QString("CORE_")
#define TORC_GUI                   QString("GUI_")
#define TORC_AUDIO                 QString("AUDIO_")
#define TORC_VIDEO                 QString("VIDEO_")
#define TORC_DB                    QString("DB_")

#endif // TORCLOCALDEFS_H
