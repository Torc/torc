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

// Qt
#include <QDomElement>
#include <QKeyEvent>

// Torc
#include "torclocalcontext.h"
#include "torclogging.h"
#include "uitext.h"
#include "uitexteditor.h"

/*! \class UITextEditor
 *  \brief A text entry widget.
 *
 * \todo Size limit
 * \todo Adjust cursor width for highlighted character (or crop)
 * \todo Remove focusable code duplicated with UIButton
 * \todo Multi-line (or new class)
*/

int UITextEditor::kUITextEditorType = UIWidget::RegisterWidgetType();

static QScriptValue UITextEditorConstructor(QScriptContext *context, QScriptEngine *engine)
{
    return WidgetConstructor<UITextEditor>(context, engine);
}

UITextEditor::UITextEditor(UIWidget *Root, UIWidget *Parent, const QString &Name, int Flags)
  : UIWidget(Root, Parent, Name, Flags),
    m_textWidget(NULL),
    m_cursorPosition(0),
    m_cursor(NULL)
{
    m_type = kUITextEditorType;
    m_focusable = true;

    if (m_parent && !m_template)
        m_parent->IncreaseFocusableChildCount();
}

UITextEditor::~UITextEditor()
{
    if (m_parent && !m_template)
        m_parent->DecreaseFocusableChildCount();

    if (m_textWidget)
        m_textWidget->DownRef();

    if (m_cursor)
        m_cursor->DownRef();
}

QString UITextEditor::GetText(void)
{
    return m_text;
}

void UITextEditor::SetText(const QString &Text)
{
    if (m_text != Text)
    {
        m_text = Text;
        if (m_textWidget)
            m_textWidget->SetText(m_text);

        emit TextChanged();
    }

    UpdateCursor();
}

bool UITextEditor::HandleAction(int Action)
{
    if (Action == Torc::Left)
    {
        if (m_cursorPosition > 0)
        {
            m_cursorPosition--;
            UpdateCursor();
        }
        return true;
    }
    else if (Action == Torc::Right)
    {
        if (m_cursorPosition < m_text.size())
        {
            m_cursorPosition++;
            UpdateCursor();
        }
        return true;
    }
    else if (Action == Torc::BackSpace)
    {
        if (m_cursorPosition > 0 && m_text.size())
        {
            QString newtext(m_text);
            newtext.remove(m_cursorPosition - 1, 1);
            m_cursorPosition--;
            SetText(newtext);
        }
        return true;
    }

    if (m_parent)
        return m_parent->HandleAction(Action);

    return false;
}

bool UITextEditor::HandleTextInput(QKeyEvent *Event)
{
    if (!Event)
        return false;

    // For consistency with UIActions::GetActionFromKey, only handle KeyPress
    if (Event->type() == QEvent::KeyRelease)
        return false;

    LOG(VB_GUI, LOG_DEBUG, QString("KeyPress %1 (%2)")
        .arg(Event->key(), 0, 16).arg(Event->text()));

    // Event must produce printable text and, in the case of simulated kepresses,
    // must be marked as printable. For a typical basic remote, this *should* mean
    // that only 0-9 are handled here..
    // As ever, this approach will probably need re-visiting to handle remapping
    // and remote input methods where printable/non-printable cannot be flagged
    if (Event->modifiers() == TORC_KEYEVENT_MODIFIERS)
        return false;

    QString text = Event->text();
    if (text.isEmpty())
        return false;

    QChar character = text.at(0);
    if (!character.isPrint())
        return false;

    QString newtext(m_text);
    newtext.insert(m_cursorPosition, text);
    m_cursorPosition += text.size();
    SetText(newtext);

    return true;
}

bool UITextEditor::Finalise(void)
{
    if (m_template)
        return true;

    if (!m_textWidget)
    {
        UIWidget *text = FindChildByName("text");
        if (text && text->Type() == UIText::kUITextType)
            m_textWidget = static_cast<UIText*>(text);
    }

    if (!m_cursor && m_textWidget)
    {
        m_cursor = FindChildByName("cursor", true);
        if (m_cursor)
        {
            connect(this, SIGNAL(Selected()),   m_cursor, SLOT(Show()));
            connect(this, SIGNAL(Deselected()), m_cursor, SLOT(Hide()));
            if (!m_selected)
                m_cursor->Hide();
        }
    }

    UpdateCursor();

    return UIWidget::Finalise();
}

void UITextEditor::UpdateCursor(void)
{
    if (!m_cursor || !m_textWidget || !m_rootParent)
        return;

    if (m_cursorPosition > m_text.size())
        m_cursorPosition = m_text.size();

    if (m_cursorPosition < 0)
        m_cursorPosition = 0;

    qreal top    = m_textWidget->GetPosition().y() * m_rootParent->GetYScale();
    QString text = m_text.left(m_cursorPosition);
    qreal left   = m_textWidget->GetWidth(text);
    int flags    = m_textWidget->GetFlags();

    if (flags & Qt::AlignRight || flags & Qt::AlignHCenter)
    {
        qreal adj = (m_textWidget->GetSize().width() * m_rootParent->GetXScale()) -
                     m_textWidget->GetWidth(m_text);
        if (flags & Qt::AlignHCenter)
            adj /= 2.0f;
        left += adj;
    }

    m_cursor->SetPosition(left / m_rootParent->GetXScale(), top);
}

void UITextEditor::SetFocusable(bool Focusable)
{
    if (Focusable)
    {
        if (!m_focusable)
            m_parent->IncreaseFocusableChildCount();
    }
    else
    {
        if (m_focusable)
            m_parent->DecreaseFocusableChildCount();

        if (m_parent && m_parent->GetLastChildWithFocus() == this)
            m_parent->SetLastChildWithFocus(NULL);

        if (GetFocusWidget() == this)
        {
            Deselect();

            // TODO get index from parent
            AutoSelectFocusWidget(0);
        }
    }

    m_focusable = Focusable;
}

void UITextEditor::CopyFrom(UIWidget *Other)
{
    UITextEditor *text = dynamic_cast<UITextEditor*>(Other);

    if (!text)
    {
        LOG(LOG_INFO, LOG_ERR, "Failed to cast widget to UITextEditor");
        return;
    }

    // NB IsFocusable returns false for a template
    m_focusable = text->m_focusable;

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
    if (Element->tagName() == "state")
    {
        // NB don't return true to allow UIWidget to parse base state
        QString focus  = Element->attribute("focusable");

        if (!focus.isEmpty())
            SetFocusable(GetBool(focus));
    }

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

