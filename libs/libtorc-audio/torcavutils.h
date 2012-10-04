#ifndef TORCAVUTILS_H
#define TORCAVUTILS_H

// Qt
#include <QString>

// Torc
#include "torcaudioexport.h"

QString TORC_AUDIO_PUBLIC AVCodecToString     (int    Codec);
QString TORC_AUDIO_PUBLIC AVMediaTypeToString (int    Type);
QString TORC_AUDIO_PUBLIC AVErrorToString     (int    Error);
QString TORC_AUDIO_PUBLIC AVTimeToString      (qint64 Time);

#endif // TORCAVUTILS_H
