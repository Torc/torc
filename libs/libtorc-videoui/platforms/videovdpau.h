#ifndef VIDEOVDPAU_H
#define VIDEOVDPAU_H

// Qt
#include <QList>
#include <QMutex>
#include <QAtomicInt>

// Torc
#include "torcreferencecounted.h"
#include "nvidiavdpau.h"

extern "C" {
#include "libavutil/pixfmt.h"
#include "libavformat/avformat.h"
#include "libavcodec/vdpau.h"
}

class NVInterop;

class VDPAUDecoderCapability
{
  public:
    VDPAUDecoderCapability();
    VDPAUDecoderCapability(VdpDecoderProfile Profile, uint32_t MaxLevel, uint32_t MaxMacroblocks, uint32_t MaxWidth, uint32_t MaxHeight);
    bool IsSupported  (uint32_t Width, uint32_t Height);

    VdpDecoderProfile m_profile;
    uint32_t          m_maxLevel;
    uint32_t          m_maxMacroblocks;
    uint32_t          m_minWidth;
    uint32_t          m_minHeight;
    uint32_t          m_maxWidth;
    uint32_t          m_maxHeight;
};

class VideoVDPAU : protected TorcReferenceCounter
{
  public:
    enum CallbackAction
    {
        NoAction = 0,
        CreateFull,
        Destroy,
        Recreate
    };

    typedef enum State
    {
        Created   = 0,
        Test,
        Full,
        Deleting,
        Deleted,
        Errored
    } State;

  public:
    static bool                       VDPAUAvailable         (void);
    static bool                       CheckHardwareSupport   (AVCodecContext *Context);
    static VideoVDPAU*                Create                 (AVCodecContext *Context);
    static void                       VDPAUUICallback        (void* Object, int Parameter);

  public:
    ~VideoVDPAU();

    void                              Preempted              (void);
    void                              Decode                 (const AVFrame *Avframe);
    struct vdpau_render_state*        GetSurface             (void);
    void                              Wake                   (void);
    bool                              Dereference            (void);
    void                              SetDeleting            (void);
    bool                              IsDeletingOrErrored    (void);

  protected:
    VideoVDPAU                        (AVCodecContext *Context);
    bool                              CreateTestVDPAU        (void);
    bool                              CreateFullVDPAU        (void);

  private:
    bool                              CreateDevice           (void);
    bool                              CreateDecoder          (void);
    bool                              CreateVideoSurfaces    (void);
    bool                              GetProcs               (void);
    bool                              RegisterCallback       (bool Register);
    bool                              CheckHardwareSupport   (void);
    bool                              IsFormatSupported      (AVCodecContext *Context);

    void                              TearDown               (void);
    void                              DestroyDevice          (void);
    void                              DestroyDecoder         (void);
    void                              DestroyVideoSurfaces   (void);
    void                              ResetProcs             (void);

    bool                              HandlePreemption       (void);

  private:
    State                             m_state;
    QAtomicInt                        m_preempted;
    QMutex                           *m_callbackLock;
    QWaitCondition                    m_callbackWait;
    AVCodecContext                   *m_avContext;
    NVInterop                        *m_nvidiaInterop;
    Display                          *m_display;
    bool                              m_createdDisplay;
    VdpDevice                         m_device;
    NVVDPAUFeatureSet                 m_featureSet;
    VdpDecoder                        m_decoder;
    QHash<VdpDecoderProfile,VDPAUDecoderCapability> m_decoderCapabilities;

    QList<struct vdpau_render_state*> m_createdVideoSurfaces;
    QList<struct vdpau_render_state*> m_allocatedVideoSurfaces;

    VdpGetProcAddress                *m_getProcAddress;
    VdpGetErrorString                *m_getErrorString;
    VdpGetApiVersion                 *m_getApiVersion;
    VdpGetInformationString          *m_getInformationString;

    VdpDeviceDestroy                 *m_deviceDestroy;
    VdpPreemptionCallbackRegister    *m_preemptionCallbackRegister;

    VdpDecoderCreate                 *m_decoderCreate;
    VdpDecoderDestroy                *m_decoderDestroy;
    VdpDecoderRender                 *m_decoderRender;
    VdpDecoderQueryCapabilities      *m_decoderQueryCapabilities;
    VdpVideoSurfaceCreate            *m_videoSurfaceCreate;
    VdpVideoSurfaceDestroy           *m_videoSurfaceDestroy;
};

#endif // VIDEOVDPAU_H
