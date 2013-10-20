#ifndef TORCAVUTILS_H
#define TORCAVUTILS_H

// Qt
#include <QString>

// Torc
#include "torcaudioexport.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

QString         TORC_AUDIO_PUBLIC AVCodecToString            (int    Codec);
QString         TORC_AUDIO_PUBLIC AVMediaTypeToString        (int    Type);
QString         TORC_AUDIO_PUBLIC AVErrorToString            (int    Error);
QString         TORC_AUDIO_PUBLIC AVTimeToString             (qint64 Time);
bool            TORC_AUDIO_PUBLIC IsHardwarePixelFormat      (AVPixelFormat Format);
bool            TORC_AUDIO_PUBLIC AVPixelFormatIsFullScale   (AVPixelFormat Format);
AVPixelFormat   TORC_AUDIO_PUBLIC AVPixelFormatFromFullScale (AVPixelFormat Format);

#endif // TORCAVUTILS_H
