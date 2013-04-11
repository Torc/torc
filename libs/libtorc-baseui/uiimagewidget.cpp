/* Class UIImageWidget
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
#include <QDomNode>

// Torc
#include "torclogging.h"
#include "uiimage.h"
#include "uiwindow.h"
#include "uitheme.h"
#include "uiimagetracker.h"
#include "uiimagewidget.h"

int UIImageWidget::kUIImageWidgetType = UIWidget::RegisterWidgetType();

static QScriptValue UIImageConstructor(QScriptContext *context, QScriptEngine *engine)
{
    return WidgetConstructor<UIImageWidget>(context, engine);
}

UIImageWidget::UIImageWidget(UIWidget *Root, UIWidget *Parent, const QString &Name, int Flags)
  : UIWidget(Root, Parent, Name, Flags),
    m_fileName(""),
    m_image(NULL),
    file("")
{
    m_type = kUIImageWidgetType;
}

UIImageWidget::~UIImageWidget()
{
    if (m_image)
        m_image->DownRef();
}

QString UIImageWidget::GetFile(void)
{
    return m_fileName;
}

void UIImageWidget::SetFile(const QString &File)
{
    if (File != m_fileName)
    {
        m_fileName = File;
        UpdateFile();
    }
}

void UIImageWidget::UpdateFile(void)
{
    if (m_image)
        m_image->DownRef();
    m_image = NULL;

    UITheme* theme = static_cast<UITheme*>(m_rootParent);
    if (theme)
        m_fileName = theme->GetDirectory() + "/" + m_fileName;
}

bool UIImageWidget::InitialisePriv(QDomElement *Element)
{
    if (!Element)
        return false;

    if (Element->tagName() == "filename")
    {
        m_fileName = GetText(Element);
        UpdateFile();
        return true;
    }

    return false;
}

void UIImageWidget::CopyFrom(UIWidget *Other)
{
    UIImageWidget *image = dynamic_cast<UIImageWidget*>(Other);

    if (!image)
    {
        LOG(LOG_INFO, LOG_ERR, "Failed to cast widget to UIImageWidget.");
        return;
    }

    // don't prepend the directory again
    m_fileName = image->GetFile();

    UIWidget::CopyFrom(Other);
}

UIWidget* UIImageWidget::CreateCopy(UIWidget* Parent, const QString &Newname)
{
    UIImageWidget* image = new UIImageWidget(m_rootParent, Parent,
                                             Newname.isEmpty() ? GetDerivedWidgetName(Parent->objectName()) : Newname,
                                             WidgetFlagNone);
    image->CopyFrom(this);
    return image;
}

bool UIImageWidget::DrawSelf(UIWindow *Window, qreal XOffset, qreal YOffset)
{
    if (!Window)
        return false;

    if (!m_image)
    {
        UIImageTracker* tracker = dynamic_cast<UIImageTracker*>(Window);
        if (tracker && !m_fileName.isEmpty())
        {
            QSize size(m_scaledRect.width(), m_scaledRect.height());
            m_image = tracker->AllocateImage(objectName(), size, m_fileName);
        }

        if (!m_image)
            return false;
    }

    Window->DrawImage(m_effect, &m_scaledRect, m_positionChanged, m_image);
    return true;
}

static class UIImageWidgetFactory : public WidgetFactory
{
    void RegisterConstructor(QScriptEngine *Engine)
    {
        if (!Engine)
            return;

        QScriptValue constructor = Engine->newFunction(UIImageConstructor);
        Engine->globalObject().setProperty("UIImage", constructor);
    }

    UIWidget* Create(const QString &Type, UIWidget *Root, UIWidget *Parent,
                     const QString &Name, int Flags)
    {
        if (Type == "image")
            return new UIImageWidget(Root, Parent, Name, Flags);
        return NULL;
    }

} UIImageWidgetFactory;
