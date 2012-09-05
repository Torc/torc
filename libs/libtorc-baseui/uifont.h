#ifndef UIFONT_H
#define UIFONT_H

// Qt
#include <QFont>
#include <QColor>
#include <QBrush>

// Torc
#include "torcreferencecounted.h"
#include "torcbaseuiexport.h"

class UIImage;

#define FONT_HEIGHT_FACTOR 0.83
#define FONT_INVERT 0x10000

class TORC_BASEUI_PUBLIC UIFont : public TorcReferenceCounter
{
  public:
    UIFont();
    virtual ~UIFont();

    void       CopyFrom         (UIFont *Other);

    QFont&     GetFace          (void);
    QColor     GetColor         (void) const;
    QBrush     GetBrush         (void) const;
    void       GetShadow        (QPoint &Offset, QColor &Color, int &FixedBlur) const;
    void       GetOutline       (QColor &Color,  quint16 &Size) const;
    QPoint     GetOffset        (void) const;
    QString    GetHash          (void) const;
    float      GetRelativeSize  (void) const;
    int        GetStretch       (void) const;
    QString    GetImageFileName (void) const;
    UIImage*   GetImage         (void) const;
    bool       GetImageReady    (void) const;

    void       SetFace          (const QFont &Face);
    void       SetColor         (const QColor &Color);
    void       SetBrush         (const QBrush &Brush);
    void       SetShadow        (bool  On, const QPoint &Offset, const QColor &Color, int FixedBlur);
    void       SetOutline       (bool  On, const QColor &Color,  quint16 Size);
    void       SetOffset        (const QPoint &Offset);
    void       SetRelativeSize  (float Size);
    void       SetSize          (int   Size);
    void       SetStretch       (int   Stretch);
    void       SetImage         (UIImage *Image);
    void       SetImageFileName (const QString &FileName);
    void       SetImageReady    (bool  Ready);

    bool       HasShadow        (void);
    bool       HasOutline       (void);

    void       UpdateTexture    (void);

  private:
    void       CalculateHash    (void);

  private:
    QString    m_hash;
    QFont      m_face;
    QBrush     m_brush;
    bool       m_hasShadow;
    QPoint     m_shadowOffset;
    QColor     m_shadowColor;
    int        m_shadowFixedBlur;
    bool       m_hasOutline;
    QColor     m_outlineColor;
    quint16    m_outlineSize;
    QPoint     m_offset;
    float      m_relativeSize;
    quint16    m_stretch;
    UIImage   *m_image;
    QString    m_imageFileName;
    bool       m_imageReady;
};

#endif // UIFONT_H
