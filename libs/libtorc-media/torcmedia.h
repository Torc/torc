#ifndef TORCMEDIA_H
#define TORCMEDIA_H

// Qt
#include <QObject>

// Torc
#include "torcreferencecounted.h"
#include "torcmediaexport.h"

class TorcMetadata;

enum TorcMediaType {
    kMediaTypeNone = 0,
    kMediaTypeGeneric,
    kMediaTypeTVEpisode,
    kMediaTypeMovie,
    kMediaTypeAdultMovie,
    kMediaTypeHomeMovie,
    kMediaTypeMusicVideo,
    kMediaTypeMusic,
    kMediaTypeAudiobook
};

class TORC_MEDIA_PUBLIC TorcMedia : public TorcReferenceCounter
{
  public:
    TorcMedia(const QString Name = "", const QString URL = "", TorcMediaType Type = kMediaTypeNone,
              TorcMetadata *Metadata = NULL);
    virtual ~TorcMedia();

    QString       GetName (void);
    QString       GetURL  (void);
    TorcMetadata* GetMetadata (void);
    TorcMediaType GetMediaType (void);

    void          SetName (const QString &Name);
    void          SetURL  (const QString &Name);
    void          SetMetadata (TorcMetadata *Metadata);
    void          SetMediaType (TorcMediaType Type);

  private:
    QString       m_name;
    QString       m_url;
    TorcMediaType m_type;
    TorcMetadata* m_metadata;
};

#endif // TORCMEDIA_H
