#ifndef UIMESSENGER_H
#define UIMESSENGER_H

// Torc
#include "uiwidget.h"

class UIGroup;

class UIMessenger : public UIWidget
{
    Q_OBJECT

  public:
    static  int     kUIMessengerType;
    UIMessenger(UIWidget *Root, UIWidget* Parent, const QString &Name, int Flags);
    virtual         ~UIMessenger();

    UIWidget*       CreateCopy         (UIWidget *Parent, const QString &Newname = QString(""));
    bool            Finalise           (void);

  protected:
    bool            event              (QEvent      *Event);


  private:
    QMap<int,UIWidget*> m_timeouts;
    UIWidget           *m_messageTemplate;
    UIWidget           *m_messageGroup;

  private:
    Q_DISABLE_COPY(UIMessenger);
};

Q_DECLARE_METATYPE(UIMessenger*);
#endif // UIMESSENGER_H
