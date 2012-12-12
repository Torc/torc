#ifndef UITEXT_H
#define UITEXT_H

// Torc
#include "torcbaseuiexport.h"
#include "uiwidget.h"

class UIFont;
class UIImage;

class TORC_BASEUI_PUBLIC UIText : public UIWidget
{
    Q_OBJECT

  public:
    static  int  kUITextType;
    UIText(UIWidget *Root, UIWidget* Parent, const QString &Name, int Flags);
    virtual ~UIText();

    virtual bool DrawSelf    (UIWindow* Window, qreal XOffset, qreal YOffset);
    virtual bool Finalise    (void);
    virtual void CopyFrom    (UIWidget *Other);

    Q_PROPERTY  (QString text  READ GetText  WRITE SetText)
    Q_PROPERTY  (QString font  READ GetFont  WRITE SetFont)
    Q_PROPERTY  (int     flags READ GetFlags WRITE SetFlags)

  public slots:
    QString      GetText     (void);
    QString      GetFont     (void);
    int          GetFlags    (void);
    void         SetText     (const QString &Text);
    void         SetFont     (const QString &Font);
    void         SetFlags    (int Flags);
    void         AddFlag     (int Flag);
    void         RemoveFlag  (int Flag);
    void         SetBlur     (int Radius);

  protected:
    virtual bool InitialisePriv (QDomElement *Element);
    virtual void CreateCopy     (UIWidget *Parent);

    void         UpdateFont     (void);

  protected:
    QString      m_text;
    QString      m_fontName;
    UIFont      *m_font;
    int          m_flags;
    int          m_blur;
    UIImage     *m_fallback;

  protected:
    QString      text;
    QString      font;
    int          flags;

  private:
   Q_DISABLE_COPY(UIText)

};

Q_DECLARE_METATYPE(UIText*);

#endif // UITEXT_H
