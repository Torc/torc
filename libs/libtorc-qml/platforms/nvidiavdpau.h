#ifndef NVIDIAVDPAU_H
#define NVIDIAVDPAU_H

// Qt
#include <QString>
#include <QOpenGLContext>

typedef GLintptr        InteropSurface;
typedef void           (APIENTRY * TORC_INITNV)     (const void*, const void*);
typedef void           (APIENTRY * TORC_FININV)     (void);
typedef InteropSurface (APIENTRY * TORC_REGOUTSURF) (const void*, GLenum, GLsizei, const uint*);
typedef GLboolean      (APIENTRY * TORC_ISSURFACE)  (InteropSurface);
typedef void           (APIENTRY * TORC_UNREGSURF)  (InteropSurface);
typedef void           (APIENTRY * TORC_SURFACCESS) (InteropSurface, GLenum);
typedef void           (APIENTRY * TORC_MAPSURF)    (GLsizei, const InteropSurface *);
typedef void           (APIENTRY * TORC_UNMAPSURF)  (GLsizei, const InteropSurface *);

class NVInterop
{
  public:
    NVInterop();
    bool               IsValid      (void);
    bool               MapFrame     (void);
    bool               UnmapFrame   (void);
    bool               Initialise   (void* Device, void* GetProcAddress);
    void               Deinitialise (void);
    void               Register     (void* Surface, GLuint Type, GLuint *Value);
    void               Unregister   (void);

  private:
    bool               m_valid;
    bool               m_initialised;
    InteropSurface     m_registeredSurface;
    TORC_INITNV        m_init;
    TORC_FININV        m_fini;
    TORC_REGOUTSURF    m_registerOutputSurface;
    TORC_ISSURFACE     m_isSurface;
    TORC_UNREGSURF     m_unregisterSurface;
    TORC_SURFACCESS    m_surfaceAccess;
    TORC_MAPSURF       m_mapSurface;
    TORC_UNMAPSURF     m_unmapSurface;
};

enum NVVDPAUFeatureSet
{
    Set_Unknown = -1,
    Set_None    = 0,
    Set_A1,
    Set_A,
    Set_B1,
    Set_B,
    Set_C,
    Set_D
};

NVVDPAUFeatureSet GetNVIDIAFeatureSet (int PCIID);
QString           FeatureSetToString  (NVVDPAUFeatureSet Set);

#endif // NVIDIAVDPAU_H
