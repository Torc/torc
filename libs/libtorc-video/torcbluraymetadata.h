#ifndef TORCBLURAYMETADATA_H
#define TORCBLURAYMETADATA_H

// Qt
#include <QString>

// Torc
#include "torcvideoexport.h"

// libbluray
#include "libbluray/src/libbluray/bdnav/meta_data.h"

class TORC_VIDEO_PUBLIC TorcBlurayMetadata
{
  public:
    TorcBlurayMetadata(const QString &Path);
    ~TorcBlurayMetadata();

    meta_dl* GetMetadata (const char *Language);

  private:
    QList<meta_dl*> m_metadata;
};

#endif // TORCBLURAYMETADATA_H
