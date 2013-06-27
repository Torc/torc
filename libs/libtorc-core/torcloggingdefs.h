#ifndef TORCLOGGINGDEFS_H_
#define TORCLOGGINGDEFS_H_

#undef VERBOSE_PREAMBLE
#undef VERBOSE_POSTAMBLE
#undef VERBOSE_MAP

#undef LOGLEVEL_PREAMBLE
#undef LOGLEVEL_POSTAMBLE
#undef LOGLEVEL_MAP

#ifdef _IMPLEMENT_VERBOSE

// This is used to actually implement the mask in mythlogging.cpp
#define VERBOSE_PREAMBLE
#define VERBOSE_POSTAMBLE
#define VERBOSE_MAP(name,mask,additive,help) \
    AddVerbose((uint64_t)(mask),QString(#name),(bool)(additive),QString(help));

#define LOGLEVEL_PREAMBLE
#define LOGLEVEL_POSTAMBLE
#define LOGLEVEL_MAP(name,value,shortname) \
    AddLogLevel((int)(value),QString(#name),(char)(shortname));

#else // !defined(_IMPLEMENT_VERBOSE)

// This is used to define the enumerated type (used by all files)
#define VERBOSE_PREAMBLE \
    enum VerboseMask {
#define VERBOSE_POSTAMBLE \
        VB_LAST_ITEM \
    };
#define VERBOSE_MAP(name,mask,additive,help) \
    name = (uint64_t)(mask),

#define LOGLEVEL_PREAMBLE \
    typedef enum {
#define LOGLEVEL_POSTAMBLE \
    } LogLevel;
#define LOGLEVEL_MAP(name,value,shortname) \
    name = (int)(value),

#endif

VERBOSE_PREAMBLE
VERBOSE_MAP(VB_ALL,       ~0ULL, false,
            "ALL available debug output")
VERBOSE_MAP(VB_GENERAL,   0x00000001, true,
            "General info")
VERBOSE_MAP(VB_RECORD,    0x00000002, true,
            "Recording related messages")
VERBOSE_MAP(VB_PLAYBACK,  0x00000004, true,
            "Playback related messages")
VERBOSE_MAP(VB_CHANNEL,   0x00000008, true,
            "Channel related messages")
VERBOSE_MAP(VB_FILE,      0x00000010, true,
            "File and AutoExpire related messages")
VERBOSE_MAP(VB_SCHEDULE,  0x00000020, true,
            "Scheduling related messages")
VERBOSE_MAP(VB_NETWORK,   0x00000040, true,
            "Network protocol related messages")
VERBOSE_MAP(VB_COMMFLAG,  0x00000080, true,
            "Commercial detection related messages")
VERBOSE_MAP(VB_AUDIO,     0x00000100, true,
            "Audio related messages")
VERBOSE_MAP(VB_LIBAV,     0x00000200, true,
            "Enables libav debugging")
VERBOSE_MAP(VB_JOBQUEUE,  0x00000400, true,
            "JobQueue related messages")
VERBOSE_MAP(VB_SIPARSER,  0x00000800, true,
            "Siparser related messages")
VERBOSE_MAP(VB_EIT,       0x00001000, true,
            "EIT related messages")
VERBOSE_MAP(VB_DATABASE,  0x00002000, true,
            "Display all SQL commands executed")
VERBOSE_MAP(VB_DSMCC,     0x00004000, true,
            "DSMCC carousel related messages")
VERBOSE_MAP(VB_MHEG,      0x00008000, true,
            "MHEG debugging messages")
VERBOSE_MAP(VB_UPNP,      0x00010000, true,
            "UPnP debugging messages")
VERBOSE_MAP(VB_SOCKET,    0x00020000, true,
            "socket debugging messages")
VERBOSE_MAP(VB_XMLTV,     0x00040000, true,
            "xmltv output and related messages")
VERBOSE_MAP(VB_DVBCAM,    0x00080000, true,
            "DVB CAM debugging messages")
VERBOSE_MAP(VB_MEDIA,     0x00100000, true,
            "Media Manager debugging messages")
VERBOSE_MAP(VB_GUI,       0x00200000, true,
            "GUI related messages")
VERBOSE_MAP(VB_SYSTEM,    0x00400000, true,
            "External executable related messages")
VERBOSE_MAP(VB_TIMESTAMP, 0x00800000, true,
            "Conditional data driven messages")
VERBOSE_MAP(VB_FLUSH,     0x01000000, true,
            "")
/* Please do not add a description for VB_FLUSH, it
   should not be passed in via "-v". It is used to
   flush output to the standard output from console
   programs that have debugging enabled.
 */
VERBOSE_MAP(VB_STDIO,     0x02000000, true,
            "")
/* Please do not add a description for VB_STDIO, it
   should not be passed in via "-v". It is used to
   send output to the standard output from console
   programs that have debugging enabled.
 */
VERBOSE_MAP(VB_NONE,      0x00000000, false,
            "NO debug output")
VERBOSE_POSTAMBLE


LOGLEVEL_PREAMBLE
LOGLEVEL_MAP(LOG_ANY,    -1, ' ')
LOGLEVEL_MAP(LOG_EMERG,   0, '!')
LOGLEVEL_MAP(LOG_ALERT,   1, 'A')
LOGLEVEL_MAP(LOG_CRIT,    2, 'C')
LOGLEVEL_MAP(LOG_ERR,     3, 'E')
LOGLEVEL_MAP(LOG_WARNING, 4, 'W')
LOGLEVEL_MAP(LOG_NOTICE,  5, 'N')
LOGLEVEL_MAP(LOG_INFO,    6, 'I')
LOGLEVEL_MAP(LOG_DEBUG,   7, 'D')
LOGLEVEL_MAP(LOG_UNKNOWN, 8, '-')
LOGLEVEL_POSTAMBLE

#endif
