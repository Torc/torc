#ifndef AUDIOOUTPUTSETTINGS_H
#define AUDIOOUTPUTSETTINGS_H

// Qt
#include <QString>
#include <QList>

// Torc
#include "torcaudioexport.h"
#include "audioeld.h"

typedef enum
{
    FORMAT_NONE = 0,
    FORMAT_U8,
    FORMAT_S16,
    FORMAT_S24LSB,
    FORMAT_S24,
    FORMAT_S32,
    FORMAT_FLT
} AudioFormat;

typedef enum
{
    FEATURE_NONE   = 0,
    FEATURE_AC3    = 1 << 0,
    FEATURE_DTS    = 1 << 1,
    FEATURE_LPCM   = 1 << 2,
    FEATURE_EAC3   = 1 << 3,
    FEATURE_TRUEHD = 1 << 4,
    FEATURE_DTSHD  = 1 << 5,
    FEATURE_AAC    = 1 << 6
} DigitalFeature;

typedef enum
{
    PassthroughNo      = -1,
    PassthroughUnknown = 0,
    PassthroughYes     = 1
} Passthrough;

class TORC_AUDIO_PUBLIC AudioOutputSettings
{
  public:
    AudioOutputSettings(bool Invalid = false);
    ~AudioOutputSettings();

    AudioOutputSettings& operator=            (const AudioOutputSettings&);
    AudioOutputSettings *GetCleaned           (bool NewCopy = false);
    AudioOutputSettings *GetUsers             (bool NewCopy = false);

    QList<int>           GetRates             (void);
    void                 AddSupportedRate     (int Rate);
    bool                 IsSupportedRate      (int Rate);
    int                  NearestSupportedRate (int rate);
    int                  BestSupportedRate    (void);
    QList<AudioFormat>   GetFormats           (void);
    void                 AddSupportedFormat   (AudioFormat Format);
    bool                 IsSupportedFormat    (AudioFormat Format);
    AudioFormat          BestSupportedFormat  (void);
    static int           FormatToBits         (AudioFormat Format);
    static const char*   FormatToString       (AudioFormat Format);
    static int           SampleSize           (AudioFormat Format);
    void                 AddSupportedChannels (int Channels);
    bool                 IsSupportedChannels  (int Channels);
    int                  BestSupportedChannels(void);
    void                 SetPassthrough       (Passthrough NewPassthrough);
    Passthrough          CanPassthrough       (void);
    bool                 CanFeature           (DigitalFeature Feature);
    bool                 CanFeature           (unsigned int Feature);
    bool                 CanAC3               (void);
    bool                 CanDTS               (void);
    bool                 CanLPCM              (void);
    bool                 IsInvalid            (void);
    void                 SetFeature           (DigitalFeature Feature);
    void                 SetFeature           (unsigned int Feature);
    void                 SetFeature           (bool Set, DigitalFeature Feature);
    void                 SetFeature           (bool Set, int Feature);
    void                 SetBestSupportedChannels(int Channels);
    int                  GetMaxHDRate         (void);
    QString              FeaturesToString     (DigitalFeature Feature);
    QString              FeaturesToString     (void);
    static QString       GetPassthroughParams (int Codec, int CodecProfile,
                                               int &Samplerate, int &Channels,
                                               bool CanDTSHDMA);
    bool                 HasELD               (void);
    void                 SetELD               (const QByteArray &Data);
    AudioELD&            GetELD               (void);
    int                  BestSupportedChannelsELD    (void);
    int                  BestSupportedPCMChannelsELD (void);

  private:
    void                 SortSupportedChannels(void);

  private:
    Passthrough          m_passthrough;
    uint                 m_features;
    bool                 m_invalid;
    bool                 m_hasELD;
    AudioELD             m_ELD;
    QList<int>           m_sr;
    QList<int>           m_rates;
    QList<int>           m_channels;
    QList<AudioFormat>   m_sf;
    QList<AudioFormat>   m_formats;
};

#endif // AUDIOOUTPUTSETTINGS_H
