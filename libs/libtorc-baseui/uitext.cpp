/* Class UIText
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
#include <QtScript>
#include <QDomElement>

// Torc
#include "torclogging.h"
#include "uiwindow.h"
#include "uiimage.h"
#include "uitheme.h"
#include "uifont.h"
#include "uitext.h"

int UIText::kUITextType = UIWidget::RegisterWidgetType();

static QScriptValue UITextConstructor(QScriptContext *context, QScriptEngine *engine)
{
    return WidgetConstructor<UIText>(context, engine);
}

UIText::UIText(UIWidget *Root, UIWidget *Parent, const QString &Name, int Flags)
  : UIWidget(Root, Parent, Name, Flags),
    m_text(""),
    m_fontName(""),
    m_font(NULL),
    m_flags(0),
    m_blur(0),
    m_fallback(NULL),
    text(""),
    font(""),
    flags(0)
{
    m_type = kUITextType;
}

UIText::~UIText()
{
    if (m_font)
        m_font->DownRef();

    if (m_fallback)
        m_fallback->DownRef();
}

QString UIText::GetText(void)
{
    return m_text;
}

QString UIText::GetFont(void)
{
    return m_fontName;
}

int UIText::GetFlags(void)
{
    return m_flags;
}

void UIText::SetText(const QString &Text)
{
    m_text = Text;
}

void UIText::SetFont(const QString &Font)
{
    if (Font != m_fontName)
    {
        m_fontName = Font;
        UpdateFont();
    }
}

void UIText::SetFlags(int Flags)
{
    m_flags = Flags;
}

void UIText::AddFlag(int Flag)
{
    m_flags |= Flag;
}

void UIText::RemoveFlag(int Flag)
{
    m_flags &= !Flag;
}

void UIText::SetBlur(int Radius)
{
    m_blur = (int(Radius * GetXScale()));
}

bool UIText::DrawSelf(UIWindow* Window, qreal XOffset, qreal YOffset)
{
    if (!Window || !m_font)
        return false;

    UIImage* oldfallback = m_fallback;

    m_fallback = Window->DrawText(m_effect, &m_scaledRect, m_positionChanged, m_text,
                                  m_font, m_flags, m_blur, m_fallback);

    if (oldfallback)
        oldfallback->DownRef();

    if (m_fallback)
        m_fallback->UpRef();

    return true;
}

bool UIText::InitialisePriv(QDomElement *Element)
{
    if (!Element)
        return false;

    if (Element->tagName() == "font")
    {
        QString fontstring = Element->attribute("name");
        QString alignment  = Element->attribute("alignment");
        QString invert     = Element->attribute("invert");

        m_fontName = fontstring;
        font = m_fontName;

        if (!alignment.isEmpty())
            m_flags |= GetAlignment(alignment);

        if (!invert.isEmpty())
            m_flags |= GetBool(invert) ? FONT_INVERT : 0;

        flags = m_flags;

        return true;
    }
    else if (Element->tagName() == "message")
    {
        m_text = UIWidget::GetText(Element);
        text = m_text;
        return true;
    }
    else if (Element->tagName() == "fixedblur")
    {
        QString blurs = Element->attribute("radius");
        if (!blurs.isEmpty())
        {
            bool ok;
            int blur = blurs.toInt(&ok);
            if (ok)
                SetBlur(blur);
        }
        return true;
    }

    return false;
}

void UIText::CopyFrom(UIWidget *Other)
{
    UIText *text = dynamic_cast<UIText*>(Other);

    if (!text)
    {
        LOG(LOG_INFO, LOG_ERR, "Failed to cast widget to UIText.");
        return;
    }

    SetText(text->GetText());
    SetFlags(text->GetFlags());

    // don't scale twice
    m_blur = text->m_blur;

    // Avoid warning messages
    m_fontName = text->GetFont();


    UIWidget::CopyFrom(Other);
}

void UIText::CreateCopy(UIWidget* Parent)
{
    UIText* text = new UIText(m_rootParent, Parent,
                              GetDerivedWidgetName(Parent->objectName()),
                              WidgetFlagNone);
    text->CopyFrom(this);
}

bool UIText::Finalise(void)
{
    if (m_template)
        return true;

    UpdateFont();
    return UIWidget::Finalise();
}

void UIText::UpdateFont(void)
{
    if (m_rootParent)
    {
        // Don't validate the font if the theme is still loading
        UITheme* theme = static_cast<UITheme*>(m_rootParent);
        if (theme && theme->GetState() == UITheme::ThemeLoadingFull)
            return;
    }

    if (m_font)
        m_font->DownRef();

    m_font = m_rootParent->GetFont(m_fontName);

    if (!m_font)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to find font '%1'").arg(m_fontName));
        return;
    }

    m_font->UpRef();
}

static class UITextFactory : public WidgetFactory
{
    void RegisterConstructor(QScriptEngine *Engine)
    {
        if (!Engine)
            return;

        QScriptValue constructor = Engine->newFunction(UITextConstructor);
        Engine->globalObject().setProperty("UIText", constructor);
    }

    UIWidget* Create(const QString &Type, UIWidget *Root, UIWidget *Parent,
                     const QString &Name, int Flags)
    {
        if (Type == "text")
            return new UIText(Root, Parent, Name, Flags);
        return NULL;
    }

} UITextFactory;
