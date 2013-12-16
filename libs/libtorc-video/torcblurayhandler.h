#ifndef TORCBLURAYHANDLER_H
#define TORCBLURAYHANDLER_H

// Qt
#include <QMutex>

// Torc
#include "torcvideoexport.h"
#include "torcbluraybuffer.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libbluray/src/libbluray/bluray.h"
#include "libbluray/src/libbluray/decoders/overlay.h"
}

class VideoDecoder;
class VideoPlayer;

class TORC_VIDEO_PUBLIC TorcBlurayHandler
{
  public:
    TorcBlurayHandler(TorcBlurayBuffer *Parent, const QString &Path, int *Abort);
    virtual ~TorcBlurayHandler();

    virtual bool           Open                 (void);
    virtual void           Close                (void);
    virtual int            Read                 (quint8 *Buffer, qint32 BufferSize);

  protected:
    void                   ProcessTitleInfo     (void);
    qreal                  GetBlurayFramerate   (quint8 Rate);

    TorcBlurayBuffer      *m_parent;
    QString                m_path;
    int                   *m_abort;
    BLURAY                *m_bluray;
    BLURAY_TITLE_INFO     *m_currentTitleInfo;
    QMutex                *m_currentTitleInfoLock;

    bool                   m_useMenus;
    bool                   m_allowBDJ;
    bool                   m_hasBDJ;
};

#endif // TORCBLURAYHANDLER_H
