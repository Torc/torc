/* Class UISetting
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2013
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Torc
#include "torclocalcontext.h"
#include "uitext.h"
#include "uibutton.h"
#include "uisetting.h"

static QScriptValue UISettingConstructor(QScriptContext *context, QScriptEngine *engine)
{
    return WidgetConstructor<UISetting>(context, engine);
}

UISetting::UISetting(UIWidget *Root, UIWidget *Parent, const QString &Name, int Flags)
  : UIGroup(Root, Parent, Name, Flags),
    m_groupTemplate(NULL)
{
}

UISetting::~UISetting()
{
    DestroySettings();
}

void UISetting::CreateSettings(void)
{
    LOG(VB_GENERAL, LOG_INFO, "Creating settings group");

    // guard against repeated calls
    DestroySettings();

    // check theme
    if (!m_groupTemplate)
    {
        LOG(VB_GENERAL, LOG_ERR, "Theme is missing settings template");
        return;
    }

    // get settings list
    QSet<TorcSetting*> settings = gRootSetting->GetChildren();

    // create items in main group
    foreach (TorcSetting *setting, settings)
    {
        if (!setting->IsActive())
            continue;

        TorcSettingGroup *group = dynamic_cast<TorcSettingGroup*>(setting);
        if (!group)
        {
            setting->DownRef();
            continue;
        }

        // get children and ignore if group if empty
        QSet<TorcSetting*> children = group->GetChildren();
        if (children.isEmpty())
        {
            setting->DownRef();
            continue;
        }

        // create a widget for the group
        QString name = objectName() + "_" + group->objectName().toLower().replace("_", "");
        UIWidget *groupwidget = m_groupTemplate->CreateCopy(this, name);
        m_settings << groupwidget;

        // name the setting
        UIWidget *settingname = groupwidget->FindChildByName("SETTINGNAME", true);
        if (settingname)
        {
            settingname = settingname->FindChildByName("text", true);
            if (settingname && settingname->Type() == UIText::kUITextType)
                dynamic_cast<UIText*>(settingname)->SetText(group->objectName());
        }

        // iterate through child settings, which will be added to SETTINGITEM
        UIWidget *settingitem = groupwidget->FindChildByName("SETTINGITEM");
        if (!settingitem)
        {
            LOG(VB_GENERAL, LOG_ERR, "Theme is missing SETTINGITEM");
            return;
        }

        foreach (TorcSetting *child, children)
            AddSetting(settingitem, child, NULL);

        // and release
        setting->DownRef();
    }
}

void UISetting::AddSetting(UIWidget *Widget, TorcSetting *Setting, UIWidget *Parent)
{
    if (!Setting)
        return;

    if (!Widget)
    {
        Setting->DownRef();
        return;
    }

    static int number = 0;
    UIWidget *newwidget = NULL;

    TorcSetting::Type type = Setting->GetSettingType();
    switch (type)
    {
        case TorcSetting::Checkbox:
            {
                UIWidget *checkboxtemplate = Widget->FindChildByName("CHECKBOX");
                if (checkboxtemplate)
                {
                    newwidget = checkboxtemplate->CreateCopy(Widget, Widget->objectName() + "_CHECKBOX" + QString::number(number++));

                    UIButton *button = dynamic_cast<UIButton*>(newwidget);
                    if (button)
                    {
                        button->SetPushed(Setting->GetValue().toBool());
                        button->SetActive(Setting->IsActive());

                        connect(Setting, SIGNAL(ValueChanged(bool)),  button,  SLOT(SetPushed(bool)));
                        connect(Setting, SIGNAL(ActiveChanged(bool)), button,  SLOT(SetActive(bool)));
                        connect(button,  SIGNAL(Pushed()),            Setting, SLOT(SetTrue()));
                        connect(button,  SIGNAL(Released()),          Setting, SLOT(SetFalse()));
                    }
                }
            }
            break;
        default:
            LOG(VB_GENERAL, LOG_ERR, "Unknown setting type");
    }

    // common setup
    if (newwidget)
    {
        // set text items
        SetDescription(newwidget, Setting);
        SetHelpText(newwidget, Setting);

        // add children
        QSet<TorcSetting*> children = Setting->GetChildren();
        foreach (TorcSetting *child, children)
            AddSetting(Widget, child, NULL);
    }

    // and release it
    Setting->DownRef();
}

void UISetting::SetDescription(UIWidget *Widget, TorcSetting *Setting)
{
    if (!Widget || !Setting)
        return;

    UIWidget *description = Widget->FindChildByName("DESCRIPTION", true);
    if (description && description->Type() == UIText::kUITextType)
        dynamic_cast<UIText*>(description)->SetText(Setting->GetName());
}

void UISetting::SetHelpText(UIWidget *Widget, TorcSetting *Setting)
{
    if (!Widget || !Setting)
        return;

    UIWidget *description = Widget->FindChildByName("HELPTEXT", true);
    if (description && description->Type() == UIText::kUITextType)
        dynamic_cast<UIText*>(description)->SetText(Setting->GetHelpText());
}

void UISetting::DestroySettings(void)
{
    while (!m_settings.isEmpty())
    {
        UIWidget* setting = m_settings.takeLast();
        RemoveChild(setting);
    }
}

bool UISetting::Finalise(void)
{
    if (m_template)
        return true;

    m_groupTemplate = FindChildByName("GROUPITEM", true);

    if (!m_groupTemplate)
    {
        LOG(VB_GENERAL, LOG_ERR, "Theme is missing settings template");
        return false;
    }

    return UIGroup::Finalise();
}

void UISetting::CopyFrom(UIWidget *Other)
{
    UISetting *setting = dynamic_cast<UISetting*>(Other);

    if (!setting)
    {
        LOG(LOG_INFO, LOG_ERR, "Failed to cast widget to UISetting");
        return;
    }

    // in case we copied the individual settings
    DestroySettings();

    UIGroup::CopyFrom(Other);
}

UIWidget* UISetting::CreateCopy(UIWidget *Parent, const QString &Newname)
{
    bool isnew = !Newname.isEmpty();
    UISetting* setting = new UISetting(m_rootParent, Parent,
                                       isnew ? Newname : GetDerivedWidgetName(Parent->objectName()),
                                       (!isnew && IsTemplate()) ? WidgetFlagTemplate : WidgetFlagNone);
    setting->CopyFrom(this);
    return setting;
}

static class UISettingFactory : public WidgetFactory
{
    void RegisterConstructor(QScriptEngine *Engine)
    {
        if (!Engine)
            return;

        QScriptValue constructor = Engine->newFunction(UISettingConstructor);
        Engine->globalObject().setProperty("UISetting", constructor);
    }

    UIWidget* Create(const QString &Type, UIWidget *Root, UIWidget *Parent,
                     const QString &Name, int Flags)
    {
        if (Type == "setting")
            return new UISetting(Root, Parent, Name, Flags);
        return NULL;
    }

} UISettingFactory;
