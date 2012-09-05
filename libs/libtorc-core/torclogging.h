#ifndef TORCLOGGING_H_
#define TORCLOGGING_H_

#ifdef __cplusplus
#include <QString>
#endif
#include <stdint.h>
#include <errno.h>

#include "torccoreexport.h"
#include "torcloggingdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VERBOSE_LEVEL_NONE (gVerboseMask == 0)
#define VERBOSE_LEVEL_CHECK(_MASK_, _LEVEL_) \
    (((gVerboseMask & (_MASK_)) == (_MASK_)) && gLogLevel >= (_LEVEL_))

#ifdef __cplusplus
#define LOG(_MASK_, _LEVEL_, _STRING_)                                  \
    do {                                                                \
        if (VERBOSE_LEVEL_CHECK((_MASK_), (_LEVEL_)) && ((_LEVEL_)>=0)) \
        {                                                               \
            PrintLogLine(_MASK_, (LogLevel)_LEVEL_,                     \
                         __FILE__, __LINE__, __FUNCTION__, 1,           \
                         QString(_STRING_).toLocal8Bit().constData());  \
        }                                                               \
    } while (false)
#else
#define LOG(_MASK_, _LEVEL_, _FORMAT_, ...)                             \
    do {                                                                \
        if (VERBOSE_LEVEL_CHECK((_MASK_), (_LEVEL_)) && ((_LEVEL_)>=0)) \
        {                                                               \
            PrintLogLine(_MASK_, (LogLevel)_LEVEL_,                     \
                         __FILE__, __LINE__, __FUNCTION__, 0,           \
                         (const char *)_FORMAT_, ##__VA_ARGS__);        \
        }                                                               \
    } while (0)
#endif

TORC_CORE_PUBLIC void PrintLogLine(uint64_t mask, LogLevel level,
                                   const char *file, int line,
                                   const char *function, int fromQString,
                                   const char *format, ...);

extern TORC_CORE_PUBLIC LogLevel gLogLevel;
extern TORC_CORE_PUBLIC uint64_t gVerboseMask;

#ifdef __cplusplus
}

extern TORC_CORE_PUBLIC QString    gLogPropagationArgs;
extern TORC_CORE_PUBLIC QString    gVerboseString;

TORC_CORE_PUBLIC void     StartLogging(QString Logfile, int progress = 0,
                                       int quiet = 0, LogLevel level = LOG_INFO,
                                       bool Propagate = false);
TORC_CORE_PUBLIC void     StopLogging(void);
TORC_CORE_PUBLIC void     CalculateLogPropagation(void);
TORC_CORE_PUBLIC bool     GetQuietLogPropagation(void);
TORC_CORE_PUBLIC LogLevel GetLogLevel(QString level);
TORC_CORE_PUBLIC QString  GetLogLevelName(LogLevel level);
TORC_CORE_PUBLIC int      ParseVerboseArgument(QString arg);
TORC_CORE_PUBLIC QString  LogErrorToString(int errnum);

/// This can be appended to the LOG args with 
/// "+".  Please do not use "<<".  It uses
/// a thread safe version of strerror to produce the
/// string representation of errno and puts it on the
/// next line in the verbose output.
#define ENO (QString("\n\t\t\teno: ") + LogErrorToString(errno))
#define ENO_STR ENO.toLocal8Bit().constData()
#endif // __cplusplus

#endif

// vim:ts=4:sw=4:ai:et:si:sts=4
