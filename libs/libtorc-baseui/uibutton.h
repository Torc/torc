#ifndef UIBUTTON_H
#define UIBUTTON_H

// Torc
#include "torcbaseuiexport.h"
#include "uiwidget.h"

class QDomElement;
class UIText;

class TORC_BASEUI_PUBLIC UIButton : public UIWidget
{
    Q_OBJECT

  public:
    static  int    kUIButtonType;
    UIButton(UIWidget *Root, UIWidget *Parent, const QString &Name, int Flags);
    virtual ~UIButton();

    virtual bool   Finalise         (void);
    virtual bool   HandleAction     (int Action);
    UIWidget*      CreateCopy       (UIWidget *Parent, const QString &Newname = QString(""));
    virtual void   CopyFrom         (UIWidget *Other);

    Q_PROPERTY    (bool    m_pushed READ IsPushed WRITE SetPushed)
    Q_PROPERTY    (QString text     READ GetText  WRITE SetText)

    Q_INVOKABLE    void SetAction   (int Action);
    Q_INVOKABLE    void SetReceiver (const QString &Receiver);
    Q_INVOKABLE    void SetReceiver (QObject *Receiver);
    Q_INVOKABLE    void SetActionData (const QVariantMap &Data);
    Q_INVOKABLE    void SetAsCheckbox (bool Checkbox);

  public slots:
    bool           IsPushed         (void);
    QString        GetText          (void);
    void           SetPushed        (bool Pushed);
    void           SetText          (QString Text);
    void           Push             (void);
    void           Release          (void);
    void           SetFocusable     (bool Focusable);

  signals:
    void           Pushed           (void);
    void           Released         (void);

  protected:
    bool           InitialisePriv   (QDomElement *Element);
    virtual void   AutoConnect      (void);

  protected:
    bool           m_pushed;
    bool           m_checkbox;
    int            m_toggled;
    UIText        *m_textWidget;
    int            m_action;
    QObject       *m_actionReceiver;
    QVariantMap    m_actionData;

  protected:
    QString        text;

  private:
    Q_DISABLE_COPY(UIButton);
};

Q_DECLARE_METATYPE(UIButton*);

#endif // UIBUTTON_H
