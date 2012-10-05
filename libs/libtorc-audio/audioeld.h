#ifndef TORCELD_H
#define TORCELD_H

// Qt
#include <QString>

// Torc
#include "torcaudioexport.h"

#define ELD_MAX_NUM_SADS 16

class TORC_AUDIO_PUBLIC AudioELD
{
  public:
    enum ELDVersions
    {
        ELD_VER_Unknown  = 0,
        ELD_VER_CEA_861D = 2,
        ELD_VER_PARTIAL  = 31
    };

    enum CEAAudioCodingTypes
    {
        TYPE_REF_STREAM_HEADER =  0,
        TYPE_LPCM              =  1,
        TYPE_AC3               =  2,
        TYPE_MPEG1             =  3,
        TYPE_MP3               =  4,
        TYPE_MPEG2             =  5,
        TYPE_AACLC             =  6,
        TYPE_DTS               =  7,
        TYPE_ATRAC             =  8,
        TYPE_SACD              =  9,
        TYPE_EAC3              = 10,
        TYPE_DTS_HD            = 11,
        TYPE_MLP               = 12,
        TYPE_DST               = 13,
        TYPE_WMAPRO            = 14,
        TYPE_REF_CXT           = 15,

        TYPE_HE_AAC            = 15,
        TYPE_HE_AAC2           = 16,
        TYPE_MPEG_SURROUND     = 17
    };

    enum CEAAudioCodingXTypes
    {
        XTYPE_HE_REF_CT      = 0,
        XTYPE_HE_AAC         = 1,
        XTYPE_HE_AAC2        = 2,
        XTYPE_MPEG_SURROUND  = 3,
        XTYPE_FIRST_RESERVED = 4
    };

    class CEASad
    {
      public:
        CEASad();

        int m_channels;
        int m_format;
        int m_rates;
        int m_sampleBits;
        int m_maxBitrate;
        int m_profile;
    };

  public:
    AudioELD(const QByteArray &ELD);
    AudioELD();
    ~AudioELD();

    static QString GetELDVersionName        (ELDVersions Version);

    bool       IsValid                      (void);
    void       Debug                        (void);
    int        GetMaxLPCMChannels           (void);
    int        GetMaxChannels               (void);
    QString    GetProductName               (void);
    QString    GetConnectionName            (void);
    QString    GetCodecsDescription         (void);

  private:
    void       Parse                        (void);
    void       ParseSAD                     (int Index, const char *Buffer);
    QString    DebugSAD                     (int Index);

  private:
    QByteArray m_eld;
    bool       m_valid;
    int        m_eldVersion;
    QString    m_monitorName;
    QString    m_connectionName;
    int        m_edidVersion;
    int        m_manufacturerId;
    int        m_productId;
    quint64    m_portId;
    quint64    m_formats;
    int        m_supportsHDCP;
    int        m_supportsAI;
    int        m_connectionType;
    int        m_audioSyncDelay;
    int        m_speakerAllocation;
    int        m_numSADs;
    CEASad     m_SADs[ELD_MAX_NUM_SADS];
};

#endif // TORCELD_H
