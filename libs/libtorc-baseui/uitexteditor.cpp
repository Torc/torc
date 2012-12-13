/* Class UITextEditor
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012
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
#include "torclogging.h"
#include "uitext.h"
#include "uitexteditor.h"

int UITextEditor::kUITextEditorType = UIWidget::RegisterWidgetType();

static QScriptValue UITextEditorConstructor(QScriptContext *context, QScriptEngine *engine)
{
    return WidgetConstructor<UITextEditor>(context, engine);
}

UITextEditor::UITextEditor(UIWidget *Root, UIWidget *Parent, const QString &Name, int Flags)
  : UIWidget(Root, Parent, Name, Flags),
    m_text(NULL),
    m_cursor(NULL)
{
    if (m_parent && !m_template)
        m_parent->IncreaseFocusableChildCount();
}

UITextEditor::~UITextEditor()
{
    if (m_parent && !m_template)
        m_parent->DecreaseFocusableChildCount();

    if (m_text)
        m_text->DownRef();

    if (m_cursor)
        m_cursor->DownRef();
}

bool UITextEditor::HandleAction(int Action)
{
    return false;
}

bool UITextEditor::Finalise(void)
{
    if (m_template)
        return true;

    if (!m_text)
    {
        UIWidget *text = FindChildByName(objectName() + "_text");
        if (text && text->Type() == UIText::kUITextType)
            m_text = static_cast<UIText*>(text);

        if (!m_text)
            return false;
    }

    if (!m_cursor)
    {
        m_cursor = FindChildByName(objectName() + "_cursor");
        if (m_cursor)
        {
            connect(this, SIGNAL(Selected()),   m_cursor, SLOT(Show()));
            connect(this, SIGNAL(Deselected()), m_cursor, SLOT(Hide()));
        }
    }

    return UIWidget::Finalise();
}

void UITextEditor::CopyFrom(UIWidget *Other)
{
    UITextEditor *text = dynamic_cast<UITextEditor*>(Other);

    if (!text)
    {
        LOG(LOG_INFO, LOG_ERR, "Failed to cast widget to UITextEditor");
        return;
    }

    UIWidget::CopyFrom(Other);
}

void UITextEditor::CreateCopy(UIWidget *Parent)
{
    UITextEditor* text = new UITextEditor(m_rootParent, Parent,
                                          GetDerivedWidgetName(Parent->objectName()),
                                          WidgetFlagNone);
    text->CopyFrom(this);

}

bool UITextEditor::InitialisePriv(QDomElement *Element)
{
    return false;
}

static class UITextEditorFactory : public WidgetFactory
{
    void RegisterConstructor(QScriptEngine *Engine)
    {
        if (!Engine)
            return;

        QScriptValue constructor = Engine->newFunction(UITextEditorConstructor);
        Engine->globalObject().setProperty("UITextEditor", constructor);
    }

    UIWidget* Create(const QString &Type, UIWidget *Root, UIWidget *Parent,
                     const QString &Name, int Flags)
    {
        if (Type == "texteditor")
            return new UITextEditor(Root, Parent, Name, Flags);
        return NULL;
    }

} UITextEditorFactory;

