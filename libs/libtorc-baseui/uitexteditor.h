#ifndef UITEXTEDITOR_H
#define UITEXTEDITOR_H

// Torc
#include "torcbaseuiexport.h"
#include "uiwidget.h"

class UIText;

class TORC_BASEUI_PUBLIC UITextEditor : public UIWidget
{
    Q_OBJECT

  public:
    static  int kUITextEditorType;
    explicit UITextEditor(UIWidget *Root, UIWidget* Parent, const QString &Name, int Flags);
    virtual ~UITextEditor();

    bool     HandleAction     (int Action);
    bool     Finalise         (void);
    void     CopyFrom         (UIWidget *Other);

  protected:
    bool     InitialisePriv   (QDomElement *Element);
    void     CreateCopy       (UIWidget *Parent);

  protected:
    UIText   *m_text;
    UIWidget *m_cursor;
};

Q_DECLARE_METATYPE(UITextEditor*);

#endif // UITEXTEDITOR_H
