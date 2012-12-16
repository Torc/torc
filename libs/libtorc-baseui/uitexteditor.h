#ifndef UITEXTEDITOR_H
#define UITEXTEDITOR_H

// Torc
#include "torcbaseuiexport.h"
#include "uiwidget.h"

class UIText;
class QKeyEvent;

class TORC_BASEUI_PUBLIC UITextEditor : public UIWidget
{
    Q_OBJECT

  public:
    static  int kUITextEditorType;
    explicit UITextEditor(UIWidget *Root, UIWidget* Parent, const QString &Name, int Flags);
    virtual ~UITextEditor();

    Q_PROPERTY (QString m_text READ GetText WRITE SetText)

    bool     HandleAction     (int Action);
    bool     HandleTextInput  (QKeyEvent *Event);
    bool     Finalise         (void);
    void     CopyFrom         (UIWidget *Other);

  public slots:
    QString  GetText          (void);
    void     SetText          (const QString &Text);

  signals:
    void     TextChanged      (void);

  protected:
    bool     InitialisePriv   (QDomElement *Element);
    void     CreateCopy       (UIWidget *Parent);
    void     UpdateCursor     (void);

  protected:
    QString   m_text;
    UIText   *m_textWidget;
    int       m_cursorPosition;
    UIWidget *m_cursor;

  private:
    Q_DISABLE_COPY(UITextEditor);
};

Q_DECLARE_METATYPE(UITextEditor*);

#endif // UITEXTEDITOR_H
