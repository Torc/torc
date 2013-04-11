#ifndef UIIMAGEWIDGET_H
#define UIIMAGEWIDGET_H

// Torc
#include "uiwidget.h"

class UIImage;

class UIImageWidget : public UIWidget
{
    Q_OBJECT

  public:
    static  int kUIImageWidgetType;
    UIImageWidget(UIWidget *Root, UIWidget *Parent, const QString &Name, int Flags);
    virtual ~UIImageWidget();

    // UIWidget
    virtual bool DrawSelf       (UIWindow* Window, qreal XOffset, qreal YOffset);
    virtual UIWidget* CreateCopy (UIWidget *Parent, const QString &Newname = QString(""));
    virtual void CopyFrom       (UIWidget *Other);

    Q_PROPERTY  (QString file READ GetFile WRITE SetFile)

  public slots:
    QString      GetFile (void);
    void         SetFile (const QString &File);

  protected:
    virtual bool InitialisePriv (QDomElement *Element);
    void         UpdateFile(void);

  protected:
    QString      m_fileName;
    UIImage     *m_image;

  protected:
    QString      file;

  private:
    Q_DISABLE_COPY(UIImageWidget)
};

Q_DECLARE_METATYPE(UIImageWidget*);

#endif // UIIMAGEWIDGET_H
