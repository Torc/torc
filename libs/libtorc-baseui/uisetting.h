#ifndef UISETTING_H
#define UISETTING_H

// Torc
#include "torcsetting.h"
#include "uiwidget.h"
#include "uigroup.h"

class UISetting : public UIGroup
{
    Q_OBJECT

  public:
    UISetting(UIWidget *Root, UIWidget* Parent, const QString &Name, int Flags);
    virtual ~UISetting();

    bool                Finalise           (void);
    UIWidget*           CreateCopy         (UIWidget *Parent, const QString &Newname = QString(""));
    void                CopyFrom           (UIWidget *Other);

  public slots:
    void                CreateSettings     (void);
    void                DestroySettings    (void);

  protected:
    void                AddSetting         (UIWidget* Widget, TorcSetting *Setting, UIWidget* Parent);
    void                SetDescription     (UIWidget* Widget, TorcSetting *Setting);
    void                SetHelpText        (UIWidget* Widget, TorcSetting *Setting);

  protected:
    QList<UIWidget*>    m_settings;
    UIWidget           *m_groupTemplate;

  private:
    Q_DISABLE_COPY(UISetting);
};

Q_DECLARE_METATYPE(UISetting*);
#endif // UISETTING_H
