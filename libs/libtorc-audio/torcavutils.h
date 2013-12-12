#ifndef TORCAVUTILS_H
#define TORCAVUTILS_H

// Qt
#include <QString>

// Torc
#include "torcaudioexport.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

#define TORC_AVERROR_RESET       (-MKTAG('R','S','e','T'))
#define TORC_AVERROR_FLUSH       (-MKTAG('F','l','S','H'))
#define TORC_AVERROR_IDLE        (-MKTAG('I','d','L','e'))
#define TORC_AVERROR_RESET_NOW   (-MKTAG('r','s','e','T'))

QString         TORC_AUDIO_PUBLIC AVCodecToString            (int    Codec);
QString         TORC_AUDIO_PUBLIC AVMediaTypeToString        (int    Type);
QString         TORC_AUDIO_PUBLIC AVErrorToString            (int    Error);
QString         TORC_AUDIO_PUBLIC AVTimeToString             (qint64 Time);
bool            TORC_AUDIO_PUBLIC IsHardwarePixelFormat      (AVPixelFormat Format);
bool            TORC_AUDIO_PUBLIC AVPixelFormatIsFullScale   (AVPixelFormat Format);
AVPixelFormat   TORC_AUDIO_PUBLIC AVPixelFormatFromFullScale (AVPixelFormat Format);

#endif // TORCAVUTILS_H
