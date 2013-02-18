#ifndef TORCOMXCORE_H
#define TORCOMXCORE_H

// Qt
#include <QLibrary>

// OpenMaxIL
#include "OMX_Types.h"
#include "OMX_Core.h"

#ifndef OMX_VERSION_MAJOR
#define OMX_VERSION_MAJOR 1
#endif
#ifndef OMX_VERSION_MINOR
#define OMX_VERSION_MINOR 1
#endif
#ifndef OMX_VERSION_REVISION
#define OMX_VERSION_REVISION 2
#endif
#ifndef OMX_VERSION_STEP
#define OMX_VERSION_STEP 0
#endif

#define OMX_INITSTRUCTURE(Struct) \
memset(&(Struct), 0, sizeof((Struct))); \
(Struct).nSize = sizeof((Struct)); \
(Struct).nVersion.s.nVersionMajor = OMX_VERSION_MAJOR; \
(Struct).nVersion.s.nVersionMinor = OMX_VERSION_MINOR; \
(Struct).nVersion.s.nRevision     = OMX_VERSION_REVISION; \
(Struct).nVersion.s.nStep         = OMX_VERSION_STEP;

typedef OMX_ERRORTYPE ( * TORC_OMXINIT)                (void);
typedef OMX_ERRORTYPE ( * TORC_OMXDEINIT)              (void);
typedef OMX_ERRORTYPE ( * TORC_OMXCOMPONENTNAMEENUM)   (OMX_STRING, OMX_U32, OMX_U32);
typedef OMX_ERRORTYPE ( * TORC_OMXGETHANDLE)           (OMX_HANDLETYPE, OMX_STRING, OMX_PTR, OMX_CALLBACKTYPE*);
typedef OMX_ERRORTYPE ( * TORC_OMXFREEHANDLE)          (OMX_HANDLETYPE);
typedef OMX_ERRORTYPE ( * TORC_OMXSETUPTUNNEL)         (OMX_HANDLETYPE, OMX_U32, OMX_HANDLETYPE, OMX_U32);
typedef OMX_ERRORTYPE ( * TORC_OMXGETCONTENTPIPE)      (OMX_HANDLETYPE, OMX_STRING);
typedef OMX_ERRORTYPE ( * TORC_OMXGETCOMPONENTSOFROLE) (OMX_STRING, OMX_U32, OMX_U8);
typedef OMX_ERRORTYPE ( * TORC_OMXGETROLESOFCOMPONENT) (OMX_STRING, OMX_U32, OMX_U8);

class TorcOMXCore : public QLibrary
{
  public:
    TorcOMXCore(const QString &Library);
   ~TorcOMXCore();

    bool IsValid (void);

    bool                        m_initialised;
    TORC_OMXINIT                m_omxInit;
    TORC_OMXDEINIT              m_omxDeinit;
    TORC_OMXCOMPONENTNAMEENUM   m_omxComponentNameEnum;
    TORC_OMXGETHANDLE           m_omxGetHandle;
    TORC_OMXFREEHANDLE          m_omxFreeHandle;
    TORC_OMXSETUPTUNNEL         m_omxSetupTunnel;
    TORC_OMXGETCONTENTPIPE      m_omxGetContentPipe;
    TORC_OMXGETCOMPONENTSOFROLE m_omxGetComponentsOfRole;
    TORC_OMXGETROLESOFCOMPONENT m_omxGetRolesOfComponent;
};

#endif // TORCOMXCORE_H
