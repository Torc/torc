#ifndef TORCMETADATA_H
#define TORCMETADATA_H

// Qt
#include <QObject>
#include <QString>

// Torc
#include "torcreferencecounted.h"
#include "torcmedia.h"
#include "torcmediaexport.h"

class TorcMedia;

class TORC_MEDIA_PUBLIC TorcMetadata : public TorcReferenceCounter
{
  public:
    TorcMetadata(TorcMedia *Media = NULL,
                 const QString Title = "",
                 const QString InternetID = "",
                 int           CollectionID = -1);

    virtual ~TorcMetadata();

    virtual TorcMedia*    GetMedia (void);
    virtual QString       GetTitle (void);
    virtual QString       GetInternetID (void);
    virtual int           GetCollectionID (void);

    virtual void          SetMedia (TorcMedia* Media);
    virtual void          SetTitle (const QString &Title);
    virtual void          SetInternetID (const QString &InternetID);
    virtual void          SetCollectionID (int CollectionID);

  private:
    TorcMedia    *m_media;
    QString       m_title;
    QString       m_internetID;
    int           m_collectionID;
};

#endif // TORCMETADATA_H
