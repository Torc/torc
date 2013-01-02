#ifndef UIIMAGEHELPER_H
#define UIIMAGEHELPER_H

// Qt
#include <QRunnable>
#include <QSize>

class UIImage;
class UIFont;
class UIImageTracker;

class UITextRenderer : public QRunnable
{
    friend class UIImageTracker;

  public:
    UITextRenderer(UIImageTracker *Parent, UIImage *Image,
                   QString Text,           QSizeF  Size,
                   const UIFont *Font,     int     Flags, int Blur);
   ~UITextRenderer();

    static void     RenderText(QPaintDevice* Device, const QString &Text,
                               UIFont *Font, int Flags, int Blur);

  protected:
    virtual void    run(void);

  private:
    UIImageTracker *m_parent;
    UIImage        *m_image;
    QString         m_text;
    QSizeF          m_size;
    UIFont         *m_font;
    int             m_flags;
    int             m_blur;
};

#endif // UIIMAGEHELPER_H
