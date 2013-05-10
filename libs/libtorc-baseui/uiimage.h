#ifndef UIIMAGE_H
#define UIIMAGE_H

// Qt
#include <QAtomicInt>
#include <QImage>

// Torc
#include "torcbaseuiexport.h"
#include "torcreferencecounted.h"

class UIImageTracker;

class TORC_BASEUI_PUBLIC UIImage : public QImage, public TorcReferenceCounter
{
    friend class UIImageTracker;

  public:
    typedef enum ImageState
    {
        ImageNull,
        ImageLoading,
        ImageLoaded,
        ImageUploadedToGPU,
        ImageReleasedFromGPU
    } ImageState;

    UIImage(UIImageTracker *Parent, const QString &Name, const QSize &MaxSize,
            const QString &FileName = QString(""));
    virtual ~UIImage();

    QString         GetName     (void);
    QString         GetFilename (void);
    void            SetState    (ImageState State);
    ImageState      GetState    (void);
    QSizeF*         GetSizeF    (void);
    QSize&          GetMaxSize  (void);
    void            FreeMem     (void);
    static void     Blur(QImage* Image, int Radius);
    int*            GetAbort    (void);
    void            SetAbort    (bool Abort);
    void            SetRawSVGData (QByteArray *Data);
    QByteArray*     GetRawSVGData (void);

  protected:
    void            Assign      (const QImage &Image);
    void            SetSizeF    (const QSizeF &Size);

  private:
    UIImageTracker *m_parent;
    ImageState      m_state;
    QString         m_name;
    QString         m_filename;
    QSizeF          m_sizeF;
    QSize           m_maxSize;
    int             m_abort;
    QByteArray     *m_rawSVGData;
};

#endif // UIIMAGE_H
