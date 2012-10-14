#ifndef TORC_VIDEO_EXP_H_
#define TORC_VIDEO_EXP_H_

// This header is called from some non-QT projects, 
// and if non C++ then Q_DECL_XXX never defined

#if defined( QT_CORE_LIB ) && defined( __cplusplus )
# include <QtCore/qglobal.h>
# ifdef TORC_VIDEO_API
#  define TORC_VIDEO_PUBLIC Q_DECL_EXPORT
# else
#  define TORC_VIDEO_PUBLIC Q_DECL_IMPORT
# endif
#else
# define TORC_VIDEO_PUBLIC
#endif

#if ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 2)))
# define THIDDEN     __attribute__((visibility("hidden")))
# define TUNUSED     __attribute__((unused))
# define TDEPRECATED __attribute__((deprecated))
#else
# define THIDDEN
# define TUNUSED
# define TDEPRECATED
#endif

#if ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 3)))
# define MERROR(x)   __attribute__((error(x)))
#else
# define MERROR(x)
#endif

#endif // TORC_VIDEO_EXP_H_
