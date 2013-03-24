/* Class UIWidget
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
* You should have received a copy of the GNU General Public License
*
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Qt
#include <QMetaMethod>
#include <QDomElement>

// Torc
#include "torclogging.h"
#include "torclocalcontext.h"
#include "uiwindow.h"
#include "uitheme.h"
#include "uieffect.h"
#include "uianimation.h"
#include "uigroup.h"
#include "uiwidget.h"

int UIWidget::kUIWidgetType = UIWidget::RegisterWidgetType();

class UIConnection
{
  public:
    UIConnection(const QString &Sender, const QString &Signal,
                 const QString &Receiver, const QString &Slot)
      : sender(Sender), signal(Signal), receiver(Receiver), slot(Slot)
    {
    }

    QString sender;
    QString signal;
    QString receiver;
    QString slot;
};

static QScriptValue UIWidgetConstructor(QScriptContext *context, QScriptEngine *engine)
{
    return WidgetConstructor<UIWidget>(context, engine);
}

static QScriptValue ScriptDebug(QScriptContext *context, QScriptEngine *engine)
{
    QString message;
    for (int i = 0; i < context->argumentCount(); ++i)
        message.append(context->argument(i).toString());

    LOG(VB_GENERAL, LOG_INFO, message);
    return engine->undefinedValue();
}

int UIWidget::RegisterWidgetType(void)
{
    static QMutex* lock = new QMutex();
    static int nexttype = 0;

    QMutexLocker locker(lock);
    return nexttype++;
}

UIWidget::UIWidget(UIWidget *Root, UIWidget* Parent, const QString &Name, int Flags)
  : QObject(Parent),
    TorcReferenceCounter(),
    m_rootParent(Root),
    m_parent(Parent),
    m_lastChildWithFocus(NULL),
    m_focusable(Flags & WidgetFlagFocusable),
    m_childHasFocus(false),
    m_focusableChildCount(0),
    m_template(Flags & WidgetFlagTemplate),
    m_decoration(Flags & WidgetFlagDecoration),
    m_unscaledRect(0.0, 0.0, 0.0, 0.0),
    m_enabled(true),
    m_visible(true),
    m_active(false),
    m_selected(false),
    m_effect(new UIEffect()),
    m_secondaryEffect(NULL),
    m_scaledRect(0.0, 0.0, 0.0, 0.0),
    m_positionChanged(true),
    m_scaleX(1.0),
    m_scaleY(1.0),
    m_clipping(false),
    alpha(1.0),
    color(1.0),
    zoom(1.0),
    verticalzoom(1.0),
    horizontalzoom(1.0),
    rotation(0.0),
    centre((int)UIEffect::Middle),
    hreflecting(false),
    hreflection(0.0),
    vreflecting(false),
    vreflection(0.0),
    position(0.0, 0.0),
    size(0.0, 0.0),
    m_engine(NULL),
    m_globalFocusWidget(NULL),
    m_direction(Torc::Down)
{
    // set the type - which MUST be overridden by a subclass
    m_type = kUIWidgetType;

    // set the widget name and register it
    setObjectName(Name);
    RegisterWidget(this);

    // register as child
    if (m_parent)
    {
        m_parent->AddChild(this);
    }
    else
    {
        // no parent, must be root
        LOG(VB_GENERAL, LOG_INFO, "Creating root widget with script engine.");

        // create our script engine
        m_engine = new QScriptEngine(this);
        connect(m_engine, SIGNAL(signalHandlerException(const QScriptValue&)),
                this, SLOT(ScriptException()));

        // make the local context available in the script environment ('LocalContext')
        AddScriptObject(gLocalContext);

        // make the main window available ('MainWindow')
        AddScriptObject(gLocalContext->GetUIObject());

        // register constructors for all known widget types
        WidgetFactory* factory = WidgetFactory::GetWidgetFactory();
        for ( ; factory; factory = factory->NextFactory())
            factory->RegisterConstructor(m_engine);

        // register Qt:: ('Qt')
        QScriptValue Qt = m_engine->newQMetaObject(&this->staticQtMetaObject);
        m_engine->globalObject().setProperty("Qt", Qt);

        // register Torc:: ('Torc')
        QScriptValue torc = m_engine->newQMetaObject(&Torc::staticMetaObject);
        m_engine->globalObject().setProperty("Torc", torc); 

        // register the log function ('Log')
        QScriptValue log = m_engine->newFunction(ScriptDebug);
        m_engine->globalObject().setProperty("Log", log);
    }

    // add this widget to the script environment
    AddScriptObject(this);
}

UIWidget::~UIWidget()
{
    // delete any outstanding connections
    while (!m_connections.isEmpty())
    {
        UIConnection* connection = m_connections.takeLast();
        delete connection;
    }

    // release fonts
    QHash<QString,UIFont*>::iterator itf = m_fonts.begin();
    for ( ; itf != m_fonts.end(); ++itf)
        itf.value()->DownRef();
    m_fonts.clear();

    // release children
    while (!m_children.isEmpty())
    {
        UIWidget* child = m_children.takeLast();
        child->DownRef();
    }

    // free up script resources
    while (!m_scriptProperties.isEmpty())
        RemoveScriptProperty(m_scriptProperties.last());

    // delete script engine
    delete m_engine;
    m_engine = NULL;

    // delete effect
    delete m_effect;
    m_effect = NULL;

    delete m_secondaryEffect;
    m_secondaryEffect = NULL;

    // unregister name
    DeregisterWidget(this);
}

bool UIWidget::Refresh(quint64 TimeNow)
{
    if (!m_enabled || m_template)
        return false;

    foreach (UIWidget* child, m_children)
        child->Refresh(TimeNow);

    return true;
}

bool UIWidget::Draw(quint64 TimeNow, UIWindow *Window, qreal XOffset, qreal YOffset)
{
    if (!m_visible || !Window || m_template)
        return false;

    XOffset += m_scaledRect.left();
    YOffset += m_scaledRect.top();

    Window->PushEffect(m_effect, &m_scaledRect);

    if (m_secondaryEffect)
    {
        if (m_clipping)
        {
            QRect clip(m_clipRect.translated(m_scaledRect.left(), m_scaledRect.top()));

            if (m_secondaryEffect->m_hReflecting)
                clip.moveTop(YOffset + m_secondaryEffect->m_hReflection + (YOffset + m_secondaryEffect->m_hReflection - clip.top() - clip.height()));
            else if (m_secondaryEffect->m_vReflecting)
                clip.moveLeft(XOffset + m_secondaryEffect->m_vReflection + (XOffset + m_secondaryEffect->m_vReflection - clip.left() - clip.width()));

            Window->PushClip(clip);
        }

        QRectF rect(0, 0, m_scaledRect.width(), m_scaledRect.height());
        Window->PushEffect(m_secondaryEffect, &rect);
        DrawSelf(Window, XOffset, YOffset);

        foreach (UIWidget* child, m_children)
            child->Draw(TimeNow, Window, XOffset, YOffset);

        if (m_clipping)
            Window->PopClip();

        Window->PopEffect();
    }

    if (m_clipping)
    {
        QRect clip(m_clipRect.translated(m_scaledRect.left(), m_scaledRect.top()));

        if (m_effect->m_hReflecting)
            clip.moveTop(YOffset + m_effect->m_hReflection + (YOffset + m_effect->m_hReflection - clip.top() - clip.height()));
        else if (m_effect->m_vReflecting)
            clip.moveLeft(XOffset + m_effect->m_vReflection + (XOffset + m_effect->m_vReflection - clip.left() - clip.width()));

        Window->PushClip(clip);
    }

    DrawSelf(Window, XOffset, YOffset);

    foreach (UIWidget* child, m_children)
        child->Draw(TimeNow, Window, XOffset, YOffset);

    if (m_clipping)
        Window->PopClip();

    Window->PopEffect();

    return true;
}

bool UIWidget::DrawSelf(UIWindow *Window, qreal XOffset, qreal YOffset)
{
    return true;
}

bool UIWidget::InitialisePriv(QDomElement *Element)
{
    return false;
}

bool UIWidget::Initialise(QDomElement *Element, const QString &Position)
{
    if (!Element)
        return false;

    if (!Position.isEmpty())
    {
        m_unscaledRect = GetRect(Position);

        if (!m_unscaledRect.isValid())
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Invalid position '%1' for '%2'")
                .arg(Position).arg(objectName()));
            return false;
        }

        m_scaledRect   = ScaleRect(m_unscaledRect);
        position       = m_scaledRect.topLeft();
        size           = m_scaledRect.size();
    }

    for (QDomNode node = Element->firstChild(); !node.isNull(); node = node.nextSibling())
    {
        QDomElement element = node.toElement();
        if (element.isNull())
            continue;

        if (InitialisePriv(&element))
        {
        }
        else if (element.tagName() == "state")
        {
            QString enabled  = element.attribute("enabled");
            QString visible  = element.attribute("visible");
            QString active   = element.attribute("active");
            QString selected = element.attribute("selected");

            if (!enabled.isEmpty())
                SetEnabled(GetBool(enabled));

            if (!visible.isEmpty())
                SetVisible(GetBool(visible));

            if (!active.isEmpty())
                SetActive(GetBool(active));

            if (!selected.isEmpty())
                SetSelected(GetBool(selected));
        }
        else if (element.tagName() == "effect")
        {
            UIEffect::ParseEffect(element, m_effect);

            QDomElement secondeffect = element.firstChildElement("effect");
            if (!secondeffect.isNull())
            {
                delete m_secondaryEffect;
                m_secondaryEffect = new UIEffect();
                UIEffect::ParseEffect(secondeffect, m_secondaryEffect);

                if (m_secondaryEffect->m_vReflecting)
                    m_secondaryEffect->m_vReflection *= m_rootParent->GetXScale();

                if (m_secondaryEffect->m_hReflecting)
                    m_secondaryEffect->m_hReflection *= m_rootParent->GetYScale();
            }

            // the line of reflection needs to be scaled for the display
            if (m_effect->m_hReflecting)
                SetHorizontalReflection(m_effect->m_hReflection);

            if (m_effect->m_vReflecting)
                SetVerticalReflection(m_effect->m_vReflection);

            alpha          = m_effect->m_alpha;
            horizontalzoom = m_effect->m_hZoom;
            verticalzoom   = m_effect->m_vZoom;
            rotation       = m_effect->m_rotation;
            centre         = m_effect->m_centre;
            hreflecting    = m_effect->m_hReflecting;
            hreflection    = m_effect->m_hReflection;
            vreflecting    = m_effect->m_vReflecting;
            vreflection    = m_effect->m_vReflection;
        }
        else if (element.tagName() == "connect")
        {
            QString sender   = element.attribute("sender").trimmed();
            QString receiver = element.attribute("receiver").trimmed();
            QString signal   = element.attribute("signal").trimmed();
            QString slot     = element.attribute("slot").trimmed();

            if (signal.isEmpty() || slot.isEmpty())
            {
                LOG(VB_GENERAL, LOG_ERR, "Connection must have a signal and slot.");
                return false;
            }

            if (sender.toLower() == "this")
                sender = QString("");

            if (receiver.toLower() == "this")
                receiver = QString("");

            m_connections.append(new UIConnection(sender, signal, receiver, slot));
        }
        else if (element.tagName() == "clip")
        {
            QString rects = element.attribute("rect");
            if (rects.isEmpty())
            {
                LOG(VB_GENERAL, LOG_ERR, "Incomplete 'clip' tag.");
            }
            else
            {
                QRectF rect = GetRect(rects);
                if (rect.isValid())
                    SetClippingRect((int)rect.left(), (int)rect.top(), (int)rect.width(), (int)rect.height());
            }
        }
        else if (element.tagName() == "scriptfunction")
        {
            QString name = element.attribute("name");
            QString args = element.attribute("arguments");
            if (name.isEmpty())
            {
                LOG(VB_GENERAL, LOG_ERR, "Scriptfunction must have a name.");
            }
            else
            {
                QString function = GetText(&element);
                function = QString("function %1(%2) {%3}").arg(name).arg(args).arg(function);

                AddScriptProperty(name, function);
            }
        }
        else
        {
            ParseWidget(m_rootParent, this, &element);
        }
    }

    return true;
}

bool UIWidget::Finalise(void)
{
    // don't process templates
    if (m_template)
        return true;

    // process connections and delete
    while (m_connections.size())
    {
        UIConnection *connection = m_connections.takeLast();
        Connect(connection->sender, connection->signal,
                connection->receiver, connection->slot);
        delete connection;
    }

    foreach (UIWidget *child, m_children)
        child->Finalise();

    AutoConnect();

    return true;
}

void UIWidget::AddFont(const QString &FontName, UIFont *Font)
{
    if (m_fonts.contains(FontName))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Font '%1' already exists - deleting.")
            .arg(FontName));
        Font->DownRef();
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Added font '%1'").arg(FontName));
    m_fonts.insert(FontName, Font);
}

UIFont* UIWidget::GetFont(const QString &FontName)
{
    if (m_fonts.contains(FontName))
        return m_fonts.value(FontName);
    return NULL;
}

bool UIWidget::ParseWidget(UIWidget *Root, UIWidget *Parent, QDomElement *Element)
{
    if (!Element)
        return false;

    QString type = Element->tagName();
    QString name = Element->attribute("name");
    QString from = Element->attribute("from");
    QString temp = Element->attribute("template");
    QString rect = Element->attribute("position");
    QString deco = Element->attribute("decoration");

    int flags = WidgetFlagNone;
    if (!temp.isEmpty() && GetBool(temp))
        flags += WidgetFlagTemplate;
    if (!deco.isEmpty() && GetBool(deco))
        flags += WidgetFlagDecoration;

    if (name.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Widget needs a name (type '%1')")
            .arg(type));
        return false;
    }

    if (from.isEmpty() && rect.isEmpty() && type != "animation")
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Widget '%1'' needs a position")
            .arg(name));
        return false;
    }

    UIWidget *newwidget = NULL;

    WidgetFactory* factory = WidgetFactory::GetWidgetFactory();
    for ( ; factory; factory = factory->NextFactory())
    {
        newwidget = factory->Create(type, Root, Parent, name, flags);
        if (newwidget)
            break;
    }

    if (!newwidget)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Unknown widget type '%1' (name '%2')")
            .arg(type).arg(name));
        return false;
    }

    if (!from.isEmpty())
    {
        UIWidget* base = Root->FindWidget(from);
        if (base)
            newwidget->CopyFrom(base);
        else
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to find base widget '%1'")
                .arg(from));
    }

    if (newwidget->Initialise(Element, rect))
        return true;

    LOG(VB_GENERAL, LOG_ERR, QString("Failed to initialise '%1' widget (name '%2')")
        .arg(type).arg(name));

    newwidget->DownRef();
    return false;
}

bool UIWidget::ParseFont(UIWidget *Root, QDomElement *Element)
{
    if (!Element || !Root)
        return false;

    QString face  = Element->attribute("face");
    QString name  = Element->attribute("name");
    QString style = Element->attribute("stylehint");
    QString from  = Element->attribute("from");

    if (name.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Font needs a name.");
        return false;
    }

    UIFont *base = NULL;
    if (!from.isEmpty())
    {
        base = Root->GetFont(from);
        if (!base)
            LOG(VB_GENERAL, LOG_WARNING, "Failed to find base font.");
    }

    if (!base && face.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Font needs a face.");
        return false;
    }

    UIFont* font = new UIFont();
    if (base)
        font->CopyFrom(base);

    if (!face.isEmpty())
        font->GetFace().setFamily(face);

    if (!style.isEmpty())
        font->GetFace().setStyleHint((QFont::StyleHint)style.toInt());

    for (QDomNode node = Element->firstChild(); !node.isNull(); node = node.nextSibling())
    {
        QDomElement element = node.toElement();
        if (element.isNull())
            continue;

        if (element.tagName() == "outline")
        {
            int width    = element.attribute("width").toInt();
            QColor color = GetQColor(element.attribute("color"));
            if (width)
                font->SetOutline(true, color, width);
        }
        else if (element.tagName() == "shadow")
        {
            QPoint offset = GetPoint(element.attribute("offset"));
            QColor color  = GetQColor(element.attribute("color"));
            QString blurs = element.attribute("fixedblur");
            int fixedblur = 0;
            if (!blurs.isEmpty())
            {
                bool ok = false;
                int blur = blurs.toInt(&ok);
                if (ok)
                    fixedblur = blur;
            }

            font->SetShadow(true, offset, color, fixedblur);
        }
        else if (element.tagName() == "spacing")
        {
            QString words   = element.attribute("words");
            QString letters = element.attribute("letters");

            if (!words.isEmpty())
                font->GetFace().setWordSpacing(words.toFloat());

            if (!letters.isEmpty())
                font->GetFace().setLetterSpacing(QFont::AbsoluteSpacing, letters.toFloat());
        }
        else if (element.tagName() == "attributes")
        {
            if (element.hasAttribute("color"))
            {
                font->SetImageReady(true);
                font->SetColor(GetQColor(element.attribute("color")));
            }
            else if (element.hasAttribute("image"))
            {
                UITheme *theme = dynamic_cast<UITheme*>(Root);
                if (theme)
                {
                    QString filename = theme->GetDirectory() + "/" + element.attribute("image").trimmed();
                    font->SetImageFileName(filename);
                }
                else
                {
                    LOG(VB_GENERAL, LOG_ERR, "Failed to cast widget to theme.");
                }
            }

            QString weight = element.attribute("weight");
            if (weight == "ultralight")
                font->GetFace().setWeight(1);
            else if (weight == "light")
                font->GetFace().setWeight(QFont::Light);
            else if (weight == "normal")
                font->GetFace().setWeight(QFont::Normal);
            else if (weight == "demibold")
                font->GetFace().setWeight(QFont::DemiBold);
            else if (weight == "bold")
                font->GetFace().setWeight(QFont::Bold);
            else if (weight == "black")
                font->GetFace().setWeight(QFont::Black);
            else if (weight == "ultrablack")
                font->GetFace().setWeight(99);
            else
                font->GetFace().setWeight(QFont::Normal);

            QString stretch = element.attribute("stretch");
            if (stretch == "ultracondensed")
                font->SetStretch(QFont::UltraCondensed);
            else if (stretch == "extracondensed")
                font->SetStretch(QFont::ExtraCondensed);
            else if (stretch == "condensed")
                font->SetStretch(QFont::Condensed);
            else if (stretch == "semicondensed")
                font->SetStretch(QFont::SemiCondensed);
            else if (stretch == "unstretched")
                font->SetStretch(QFont::Unstretched);
            else if (stretch == "semiexpanded")
                font->SetStretch(QFont::SemiExpanded);
            else if (stretch == "expanded")
                font->SetStretch(QFont::Expanded);
            else if (stretch == "extraexpanded")
                font->SetStretch(QFont::ExtraExpanded);
            else if (stretch == "ultraexpanded")
                font->SetStretch(QFont::UltraExpanded);

            QStringList decorations = element.attribute("decoration").split(',');
            foreach (QString decoration, decorations)
            {
                decoration = decoration.trimmed();
                if (decoration == "italic")
                    font->GetFace().setItalic(true);
                else if (decoration == "underline")
                    font->GetFace().setUnderline(true);
                else if (decoration == "overline")
                    font->GetFace().setOverline(true);
                else if (decoration == "strikeout")
                    font->GetFace().setStrikeOut(true);
            }

            // N.B. This comes last to ensure we get the correct size
            int size = element.attribute("size", "20").toInt();
            size = (int)(((qreal)size + 0.5) * Root->GetYScale() * FONT_HEIGHT_FACTOR);
            font->SetSize(size);
        }
    }

    Root->AddFont(name, font);

    return true;
}

QString UIWidget::GetText(QDomElement *Element)
{
    if (!Element)
        return QString();

    for (QDomNode node = Element->firstChild(); !node.isNull(); node = node.nextSibling())
    {
        QDomText text = node.toText();
        if (!text.isNull())
            return text.data();
    }
    return QString("");
}

QColor UIWidget::GetQColor(const QString &Color)
{
    if (Color.size() == 8)
    {
        bool redok;
        bool greenok;
        bool blueok;
        bool alphaok;

        int red   = Color.mid(0, 2).toInt(&redok, 16);
        int green = Color.mid(2, 2).toInt(&greenok, 16);
        int blue  = Color.mid(4, 2).toInt(&blueok, 16);
        int alpha = Color.mid(6, 2).toInt(&alphaok, 16);

        if (redok && greenok && blueok && alphaok)
            return QColor(red, green, blue, alpha);
    }

    return QColor(255, 255, 255, 255);
}

QRectF UIWidget::GetRect(QDomElement *Element)
{
    if (!Element)
        return QRectF();

    return GetRect(GetText(Element));
}

QRectF UIWidget::GetRect(const QString &Rect)
{
    QStringList values = Rect.split(',');
    if (values.size() != 4)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to parse position.");
        return QRectF();
    }

    return QRectF(values[0].toFloat(),
                  values[1].toFloat(),
                  values[2].toFloat(),
                  values[3].toFloat());
}

QRectF UIWidget::GetScaledRect(const QString &Rect)
{
    QRectF rect = GetRect(Rect);
    return ScaleRect(rect);
}

QRectF UIWidget::ScaleRect(const QRectF &Rect)
{
    return QRectF(Rect.left() * m_rootParent->GetXScale(),
                  Rect.top()  * m_rootParent->GetYScale(),
                  Rect.width() * m_rootParent->GetXScale(),
                  Rect.height() * m_rootParent->GetYScale());
}

QPoint UIWidget::GetPoint(const QString &Point)
{
    QStringList values = Point.split(',');
    if (values.size() != 2)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to parse point.");
        return QPoint(0, 0);
    }

    return QPoint(values[0].toInt(),
                  values[1].toInt());
}

QPointF UIWidget::GetPointF(const QString &Point)
{
    QStringList values = Point.split(',');
    if (values.size() != 2)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to parse point.");
        return QPointF(0, 0);
    }

    return QPointF(values[0].toFloat(), values[1].toFloat());
}

QPointF UIWidget::GetScaledPointF(const QString &Point)
{
    QPointF point = GetPointF(Point);
    return ScalePointF(point);
}

QPointF UIWidget::ScalePointF(const QPointF &Point)
{
    return QPointF(Point.x() * m_rootParent->GetXScale(),
                   Point.y() * m_rootParent->GetYScale());
}

QSizeF UIWidget::GetSizeF(const QString &Size)
{
    QStringList values = Size.split(',');
    if (values.size() != 2)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to parse size.");
        return QSizeF(0, 0);
    }

    return QSizeF(values[0].toFloat(), values[1].toFloat());
}

QSizeF UIWidget::GetScaledSizeF(const QString &Size)
{
    QSizeF size = GetSizeF(Size);
    return ScaleSizeF(size);
}

QSizeF UIWidget::ScaleSizeF(const QSizeF &Size)
{
    return QSizeF(Size.width() * m_rootParent->GetXScale(),
                  Size.height() * m_rootParent->GetYScale());
}

bool UIWidget::GetBool(QDomElement *Element)
{
    return GetBool(GetText(Element));
}

bool UIWidget::GetBool(const QString &Bool)
{
    QString bools = Bool.toLower();
    if (bools == "true" || bools == "yes")
        return true;
    else if (bools == "false" || bools == "no")
        return false;
    else
        return bools.toUInt();
}

int UIWidget::GetAlignment(const QString &Alignment)
{
    int result = Qt::AlignLeft | Qt::AlignTop;

    QStringList alignments = Alignment.toLower().split(',');

    foreach (QString alignment, alignments)
    {
        if (alignment == "center" || alignment == "allcenter")
        {
            result &= ~(Qt::AlignHorizontal_Mask | Qt::AlignVertical_Mask);
            result |= Qt::AlignCenter;
            break;
        }
        else if (alignment == "justify")
        {
            result &= ~Qt::AlignHorizontal_Mask;
            result |= Qt::AlignJustify;
        }
        else if (alignment == "left")
        {
            result &= ~Qt::AlignHorizontal_Mask;
            result |= Qt::AlignLeft;
        }
        else if (alignment == "hcenter")
        {
            result &= ~Qt::AlignHorizontal_Mask;
            result |= Qt::AlignHCenter;
        }
        else if (alignment == "right")
        {
            result &= ~Qt::AlignHorizontal_Mask;
            result |= Qt::AlignRight;
        }
        else if (alignment == "top")
        {
            result &= ~Qt::AlignVertical_Mask;
            result |= Qt::AlignTop;
        }
        else if (alignment == "vcenter")
        {
            result &= ~Qt::AlignVertical_Mask;
            result |= Qt::AlignVCenter;
        }
        else if (alignment == "bottom")
        {
            result &= ~Qt::AlignVertical_Mask;
            result |= Qt::AlignBottom;
        }
    }

    return (Qt::Alignment)result;
}

void UIWidget::CopyFrom(UIWidget *Other)
{
    // N.B. Script properties and signal/slot connections are NOT copied

    if (!Other)
        return;

    if (Other == m_rootParent)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot copy the root widget!");
        return;
    }

    // base properties
    SetEnabled(Other->IsEnabled());
    SetVisible(Other->IsVisible());
    SetActive(Other->IsActive());
    SetSelected(Other->IsSelected());
    SetAlpha(Other->GetAlpha());
    SetColor(Other->GetColor());
    SetRotation(Other->GetRotation());
    SetHorizontalZoom(Other->GetHorizontalZoom());
    SetVerticalZoom(Other->GetVerticalZoom());
    SetCentre(Other->GetCentre());

    if (Other->m_effect->m_hReflecting)
        SetHorizontalReflection(Other->m_effect->m_hReflection / Other->GetYScale());
    if (Other->m_effect->m_vReflecting)
        SetVerticalReflection(Other->m_effect->m_vReflection / Other->GetXScale());

    if (Other->m_secondaryEffect)
    {
        delete m_secondaryEffect;
        m_secondaryEffect = new UIEffect();
        *m_secondaryEffect = *Other->m_secondaryEffect;
        if (m_secondaryEffect->m_hReflecting)
            m_secondaryEffect->m_hReflection = (m_secondaryEffect->m_hReflection / Other->GetYScale()) * m_rootParent->GetYScale();
        if (m_secondaryEffect->m_vReflecting)
            m_secondaryEffect->m_vReflection = (m_secondaryEffect->m_vReflection / Other->GetXScale()) * m_rootParent->GetXScale();
    }


    m_scaleX = Other->GetXScale();
    m_scaleY = Other->GetYScale();

    // defaults
    alpha          = Other->alpha;
    color          = Other->color;
    zoom           = Other->zoom;
    verticalzoom   = Other->verticalzoom;
    horizontalzoom = Other->horizontalzoom;
    rotation       = Other->rotation;
    centre         = Other->centre;
    hreflecting    = Other->hreflecting;
    hreflection    = Other->hreflection;
    vreflecting    = Other->vreflecting;
    vreflection    = Other->vreflection;
    position       = Other->position;
    size           = Other->size;

    // avoid scaling twice
    m_scaledRect   = Other->m_scaledRect;
    m_unscaledRect = Other->m_unscaledRect;

    // connections should be empty unless Other is a template
    foreach (UIConnection* connection, Other->m_connections)
    {
        QString sender   = connection->sender;
        QString signal   = connection->signal;
        QString receiver = connection->receiver;
        QString slot     = connection->slot;

        if (sender.startsWith(Other->objectName()))
            sender = objectName() + sender.mid(Other->objectName().size());
        if (receiver.startsWith(Other->objectName()))
            receiver = objectName() + receiver.mid(Other->objectName().size());

        m_connections.append(new UIConnection(sender, signal, receiver, slot));
    }

    // children
    foreach (UIWidget* child, Other->m_children)
        child->CreateCopy(this);

    // join the dots
    Finalise();
}

void UIWidget::CreateCopy(UIWidget* Parent)
{
    UIWidget* widget = new UIWidget(m_rootParent, Parent,
                                    GetDerivedWidgetName(Parent->objectName()),
                                    WidgetFlagNone);
    widget->CopyFrom(this);
}

QString UIWidget::GetDerivedWidgetName(const QString &NewParentName)
{
    // creates newwindow_background_shape from window_background_shape
    if (objectName().startsWith(m_parent->objectName()))
        return NewParentName + objectName().mid(m_parent->objectName().size());

    LOG(VB_GENERAL, LOG_ERR, QString("Unable to determine derived object name (%1)")
        .arg(objectName()));
    return "Error";
}

void UIWidget::AddChild(UIWidget *Widget)
{
    if (!Widget)
        return;

    m_children.append(Widget);
}

void UIWidget::RemoveChild(UIWidget *Widget)
{
    int index = m_children.indexOf(Widget);

    if (index < 0)
        return;

    m_children.removeOne(Widget);

    if (m_lastChildWithFocus == Widget)
        SetLastChildWithFocus(NULL);

    if (Widget && GetFocusWidget() == Widget)
    {
        Widget->Deselect();
        AutoSelectFocusWidget(index);
    }

    Widget->DownRef();
}

void UIWidget::IncreaseFocusableChildCount(void)
{
    if (m_focusable)
        return;

    m_focusableChildCount++;

    if (m_parent)
        m_parent->IncreaseFocusableChildCount();
}

void UIWidget::DecreaseFocusableChildCount(void)
{
    if (m_focusable)
        return;

    m_focusableChildCount--;

    if (m_parent)
        m_parent->DecreaseFocusableChildCount();
}

int UIWidget::GetDirection(void)
{
    if (m_parent && m_rootParent)
        return m_rootParent->GetDirection();

    return m_direction;
}

void UIWidget::SetDirection(int Direction)
{
    m_direction = Direction;
}

void UIWidget::RegisterWidget(UIWidget *Widget)
{
    if (!Widget)
        return;

    if (m_parent && m_rootParent)
    {
        m_rootParent->RegisterWidget(Widget);
        return;
    }

    if (m_widgetNames.contains(Widget->objectName()))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Widget name '%1' is already in use.")
            .arg(Widget->objectName()));
        return;
    }

    m_widgetNames.insert(Widget->objectName(), Widget);
}

void UIWidget::DeregisterWidget(UIWidget *Widget)
{
    if (!Widget)
        return;

    if (m_parent && m_rootParent)
    {
        m_rootParent->DeregisterWidget(Widget);
        return;
    }

    int count = m_widgetNames.remove(Widget->objectName());
    if (count != 1)
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("Widget '%1' wasn't found.")
            .arg(Widget->objectName()));
    }
}

UIWidget* UIWidget::FindWidget(const QString &Name)
{
    if (m_parent && m_rootParent)
        return m_rootParent->FindWidget(Name);

    QHash<QString,UIWidget*>::iterator it = m_widgetNames.find(Name);
    if (it != m_widgetNames.end())
        return it.value();

    return NULL;
}

UIWidget* UIWidget::FindChildByName(const QString &Name, bool Recursive)
{
    foreach (UIWidget* child, m_children)
    {
        if (child->objectName() == Name)
            return child;

        if (Recursive)
        {
            UIWidget *result = child->FindChildByName(Name, true);
            if (result)
                return result;
        }
    }

    if (!m_parent)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to find widget '%1' (Recursive: %2)")
            .arg(Name).arg(Recursive));
    }

    return NULL;
}

bool UIWidget::IsFocusable(void)
{
    // FIXME this needs to be m_active and not m_visible
    return m_visible && (m_focusable || m_focusableChildCount) && !m_template;
}

bool UIWidget::IsButton(void)
{
    return m_visible && m_focusable && !m_template;
}

UIWidget* UIWidget::GetFocusWidget(void)
{
    if (m_parent && m_rootParent)
        return m_rootParent->GetFocusWidget();

    return m_globalFocusWidget;
}

void UIWidget::SetFocusWidget(UIWidget *Widget)
{
    if (m_parent && m_rootParent)
    {
        m_rootParent->SetFocusWidget(Widget);
        return;
    }

    if (m_globalFocusWidget == Widget)
        return;

    if (m_globalFocusWidget)
        m_globalFocusWidget->Deselect();

    if (Widget)
    {
        if (!Widget->IsButton())
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Selected widget cannot take focus (%1)")
                .arg(Widget->objectName()));
            m_globalFocusWidget = NULL;
            return;
        }
    }

    m_globalFocusWidget = Widget;
}

void UIWidget::SetLastChildWithFocus(UIWidget *Widget)
{
    m_lastChildWithFocus = Widget;
}

UIWidget* UIWidget::GetLastChildWithFocus(void)
{
    return m_lastChildWithFocus;
}

void UIWidget::SetHasChildWithFocus(bool HasFocus)
{
    m_childHasFocus = HasFocus;

    // TODO != UIButton::kUIButtonType might be more flexible
    // TODO or just if (m_parent)...
    if (m_parent && (m_parent->Type() == UIGroup::kUIGroupType))
        m_parent->SetHasChildWithFocus(HasFocus);
}

bool UIWidget::HasChildWithFocus(void)
{
    return m_childHasFocus;
}

bool UIWidget::AutoSelectFocusWidget(int Index)
{
    // ignore if we already have a selection
    if (GetFocusWidget() != NULL)
        return true;

    // already have something stored
    if (m_lastChildWithFocus && m_children.contains(m_lastChildWithFocus))
    {
        m_lastChildWithFocus->Select();
        return true;
    }

    // reset
    m_lastChildWithFocus = NULL;

    int size = m_children.size();

    if (!size)
        return false;

    if (Index >= size)
        Index = size - 1;

    if (Index < 0)
        Index = 0;

    // from the selected index forwards
    for (int i = Index; i < size; ++i)
    {
        if (m_children.at(i)->IsButton())
        {
            m_children.at(i)->Select();
            return true;
        }

        if (m_children.at(i)->AutoSelectFocusWidget(0))
            return true;
    }

    // and the remainder
    for (int i = Index; i >= 0; --i)
    {
        if (m_children.at(i)->IsButton())
        {
            m_children.at(i)->Select();
            return true;
        }

        if (m_children.at(i)->AutoSelectFocusWidget(0))
            return true;
    }

    return false;
}

bool UIWidget::HandleAction(int Action)
{
    if (Action == Torc::Down || Action == Torc::Right)
    {
        if (m_rootParent)
            m_rootParent->SetDirection(Action);

        int current = m_children.indexOf(m_lastChildWithFocus);
        for (int i = current + 1; i != current; ++i)
        {
            if (i >= m_children.size())
                i = 0;

            if (m_children.at(i)->IsFocusable())
            {
                SetFocusWidget(NULL);
                m_children.at(i)->Select();
                return true;
            }
        }
    }
    else if (Action == Torc::Up || Action == Torc::Left)
    {
        if (m_rootParent)
            m_rootParent->SetDirection(Action);

        int current = m_children.indexOf(m_lastChildWithFocus);
        for (int i = current - 1; i != current; --i)
        {
            if (i < 0)
                i = m_children.size() - 1;

            if (m_children.at(i)->IsFocusable())
            {
                SetFocusWidget(NULL);
                m_children.at(i)->Select();
                return true;
            }
        }
    }

    return false;
}

UIWidget* UIWidget::GetRootWidget(void)
{
    return m_rootParent;
}

int UIWidget::Type(void)
{
    return m_type;
}

void UIWidget::SetAsTemplate(bool Template)
{
    m_template = Template;
}

bool UIWidget::IsTemplate(void)
{
    return m_template;
}

bool UIWidget::IsDecoration(void)
{
    return m_decoration;
}

bool UIWidget::Connect(const QString &Sender, const QString &Signal,
                       const QString &Receiver, const QString &Slot)
{
    UIWidget* sender = Sender.isEmpty() ? this : FindWidget(Sender);
    if (!sender)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to find sender.");
        return false;
    }

    if (sender->IsTemplate())
    {
        LOG(VB_GENERAL, LOG_ERR, "Trying to send signal from template.");
        return false;
    }

    // if the function requires arguments, the theme must use the full signature
    // e.g. ValueChanged(int), otherwise append '()'
    QByteArray signal = Signal.trimmed().toLatin1();
    if (!signal.endsWith(")"))
        signal += "()";

    int signalidx = sender->metaObject()->indexOfSignal(signal);
    if (signalidx < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Widget '%1' has no signal named '%2'")
            .arg(Sender).arg(Signal));
        return false;
    }

    // FIXME - move to QMetaMethod in Qt 4.8
    signal = "2" + signal;

    UIWidget* receiver = Receiver.isEmpty() ? this : FindWidget(Receiver);
    if (!receiver)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to find receiver.");
        return false;
    }

    if (sender->IsTemplate())
    {
        LOG(VB_GENERAL, LOG_ERR, "Trying to send signal to template.");
        return false;
    }


    QByteArray slot = Slot.trimmed().toLatin1() + "()";

    int slotidx = receiver->metaObject()->indexOfSlot(slot);
    if (slotidx < 0)
    {
        // try script functions
        QScriptEngine *engine = GetScriptEngine();
        if (engine)
        {
            QScriptValue function = engine->globalObject().property(Slot.trimmed());
            if (function.isValid() && function.isFunction())
            {
                QScriptValue receivervalue = engine->globalObject().property(receiver->objectName());
                // Should this check isObject or isQObject as well?
                if (receivervalue.isValid())
                    return qScriptConnect(sender, signal, receivervalue, function);
            }

            LOG(VB_GENERAL, LOG_ERR, QString("No script function named '%1'")
                .arg(Slot));
        }

        LOG(VB_GENERAL, LOG_ERR, QString("Widget '%1' has no slot named '%2'")
            .arg(receiver->objectName()).arg(Slot));
        return false;
    }

    // FIXME - move to QMetaMethod in Qt 4.8
    slot = "1" + slot;

    return connect(sender, signal, receiver, slot);
}

void UIWidget::AutoConnect(void)
{
    foreach (UIWidget* child, m_children)
    {
        if (child->Type() != UIAnimation::kUIAnimationType)
            continue;

        QString name = child->objectName();
        if (!name.startsWith(objectName()))
            continue;

        QString signal = name.mid(objectName().size() + 1);

        if (signal == "Shown"     || signal == "Hidden" ||
            signal == "Activated" || signal == "Deactivated" ||
            signal == "Selected"  || signal == "Deselected")
        {
            Connect("", signal, name, "Start");
        }
    }
}

QScriptEngine* UIWidget::GetScriptEngine(void)
{
    if (m_engine)
        return m_engine;

    if (m_rootParent && (m_rootParent != this))
        return m_rootParent->GetScriptEngine();

    return NULL;
}

QScriptEngine* UIWidget::PreValidateProperty(const QString &Property, bool IsObject)
{
    QScriptEngine* engine = GetScriptEngine();

    if (!engine || Property.isEmpty())
        return NULL;

    QScriptValue exists = engine->globalObject().property(Property);
    if (exists.isValid())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Script property '%1' already exists - ignoring.")
            .arg(Property));
        return NULL;
    }

    if (!IsObject && FindWidget(Property))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Script property '%1' matches existing widget name - ignoring.")
            .arg(Property));
        return NULL;
    }

    return engine;
}

void UIWidget::AddScriptProperty(const QString &Property, const QString &Value)
{
    QScriptEngine* engine = PreValidateProperty(Property, false);
    if (!engine)
        return;

    QScriptValue function = engine->evaluate(Value);
    ScriptException();

    m_scriptProperties.append(Property);
}

void UIWidget::AddScriptObject(QObject *Object)
{
    if (!Object)
        return;

    QScriptEngine* engine = PreValidateProperty(Object->objectName(), true);
    if (!engine)
        return;

    engine->globalObject().setProperty(Object->objectName(), engine->newQObject(Object));
    ScriptException();

    m_scriptProperties.append(Object->objectName());
}

void UIWidget::RemoveScriptProperty(const QString &Property)
{
    QScriptEngine* engine = GetScriptEngine();
    if (!engine)
        return;

    if (!m_scriptProperties.contains(Property))
        LOG(VB_GENERAL, LOG_WARNING, QString("Property '%1' not recognised.").arg(Property));

    engine->globalObject().setProperty(Property, QScriptValue());
    m_scriptProperties.removeOne(Property);

    ScriptException();
}

void UIWidget::ScriptException(void)
{
    QScriptEngine *engine = GetScriptEngine();
    if (!engine)
        return;

    if (engine->hasUncaughtException())
    {
        QScriptValue exception = engine->uncaughtException();
        int          line      = engine->uncaughtExceptionLineNumber();
        QStringList  backtrace = engine->uncaughtExceptionBacktrace();

        LOG(VB_GENERAL, LOG_ERR, QString("Unhandled script exception at line %1").arg(line));
        LOG(VB_GENERAL, LOG_ERR, QString("Exception: %1").arg(exception.toString()));
        LOG(VB_GENERAL, LOG_ERR, QString("Backtrace:\n%1").arg(backtrace.join("\n")));

        engine->clearExceptions();
    }
}

void UIWidget::Debug(int Depth)
{
    if (!m_parent)
        LOG(VB_GUI, LOG_DEBUG, QString("%1 fonts loaded.").arg(m_fonts.size()));

    LOG(VB_GUI, LOG_DEBUG, QString("--").repeated(Depth++) + objectName());
    foreach (UIWidget* child, m_children)
        child->Debug(Depth);
}

bool UIWidget::IsEnabled(void)
{
    return m_enabled;
}

bool UIWidget::IsVisible(void)
{
    return m_visible;
}

bool UIWidget::IsActive(void)
{
    return m_active;
}

bool UIWidget::IsSelected(void)
{
    return m_selected;
}

bool UIWidget::IsPositionChanged(void)
{
    return m_positionChanged;
}

qreal UIWidget::GetAlpha(void)
{
    return m_effect->m_alpha;
}

qreal UIWidget::GetColor(void)
{
    return m_effect->m_color;
}

qreal UIWidget::GetZoom(void)
{
    return m_effect->m_hZoom; // NB
}

qreal UIWidget::GetHorizontalZoom(void)
{
    return m_effect->m_hZoom;
}

qreal UIWidget::GetVerticalZoom(void)
{
    return m_effect->m_vZoom;
}

qreal UIWidget::GetRotation(void)
{
    return m_effect->m_rotation;
}

QPointF UIWidget::GetPosition(void)
{
    return m_unscaledRect.topLeft();
}

QSizeF UIWidget::GetSize(void)
{
    return m_unscaledRect.size();
}

int UIWidget::GetCentre(void)
{
    return (int)m_effect->m_centre;
}

qreal UIWidget::GetVerticalReflection(void)
{
    return m_effect->m_vReflection / m_rootParent->GetXScale();
}

qreal UIWidget::GetHorizontalReflection(void)
{
    return m_effect->m_hReflection / m_rootParent->GetYScale();
}

qreal UIWidget::GetXScale(void)
{
    return m_scaleX;
}

qreal UIWidget::GetYScale(void)
{
    return m_scaleY;
}

void UIWidget::SetEnabled(bool Enabled)
{
    if (Enabled)
        Enable();
    else
        Disable();
}

void UIWidget::Close(void)
{
    emit Closed();

    if (m_parent)
        m_parent->RemoveChild(this);
    else
        DownRef();
}

void UIWidget::Enable(void)
{
    bool signal = !m_enabled;
    m_enabled = true;
    if (signal)
        emit Enabled();
}

void UIWidget::Disable(void)
{
    bool signal = m_enabled;
    m_enabled = false;
    Hide();
    if (signal)
        emit Disabled();
}

void UIWidget::SetVisible(bool Visible)
{
    if (Visible)
        Show();
    else
        Hide();
}

void UIWidget::Show(void)
{
    bool signal = !m_visible;
    m_visible = true;
    Enable();
    if (signal)
        emit Shown();
}

void UIWidget::Hide(void)
{
    bool signal = m_visible;
    m_visible = false;
    Deactivate();
    if (signal)
        emit Hidden();
}

void UIWidget::SetActive(bool Active)
{
    if (Active)
        Activate();
    else
        Deactivate();
}

void UIWidget::Activate(void)
{
    bool signal = !m_active;
    m_active = true;
    Show();
    if (signal)
    {
        AutoSelectFocusWidget(0);
        emit Activated();
    }
}

void UIWidget::Deactivate(void)
{
    bool signal = m_active;
    m_active = false;
    Deselect();
    if (signal)
        emit Deactivated();
}

void UIWidget::SetSelected(bool Selected)
{
    if (Selected)
        Select();
    else
        Deselect();
}

void UIWidget::Select(void)
{
    // check for groups with focusable children
    if (!m_focusable && m_focusableChildCount)
    {
        AutoSelectFocusWidget(0);
        if (m_parent)
            m_parent->SetLastChildWithFocus(this);
        return;
    }

    // action...
    bool signal = !m_selected;
    m_selected = true;
    Activate();
    if (signal)
    {
        SetFocusWidget(this);

        // update group state tracking
        if (m_parent)
        {
            m_parent->SetLastChildWithFocus(this);
            m_parent->SetHasChildWithFocus(true);
        }

        emit Selected();
    }
}

void UIWidget::Deselect(void)
{
    // action...
    bool signal = m_selected;
    m_selected = false;
    if (signal)
    {
        SetFocusWidget(NULL);

        // update group state tracking
        if (m_parent)
            m_parent->SetHasChildWithFocus(false);

        emit Deselected();
    }
}

void UIWidget::HideAll(void)
{
    foreach (UIWidget* child, m_children)
        child->HideAll();
    Hide();
}

void UIWidget::ShowAll(void)
{
    foreach (UIWidget* child, m_children)
        child->ShowAll();
    Show();
}

bool UIWidget::SetAsRootWidget(void)
{
    return SetRootWidget(this);
}

bool UIWidget::SetRootWidget(UIWidget *Widget)
{
    // FIXME - ??
    // this currently assumes a flat list of top level windows of which only
    // one can be visible. Children cannot become 'root' - gives flexibility,
    // simplifies animating between windows BUT may not be performant with a
    // large number of top level windows, requires theme/code to track
    // current widget and excludes window stack effects.
    // A UIStack class may be needed.

    if (m_rootParent && m_parent)
        return m_rootParent->SetRootWidget(Widget);

    int index = m_children.indexOf(Widget);

    if (index > -1)
    {
        // deselect focus widget
        UIWidget *focus = GetFocusWidget();
        if (focus)
            focus->Deselect();

        // find the current top widget and disable it
        foreach (UIWidget* child, m_children)
        {
            if (child->IsEnabled())
                child->Deactivate();
        }

        m_children[index]->Activate();
        return true;
    }

    return false;
}

bool UIWidget::SetRootWidget(const QString &Widget)
{
    if (m_rootParent && m_parent)
        return m_rootParent->SetRootWidget(Widget);

    // find the requested widget
    UIWidget* widget = FindChildByName(Widget, false /*recursive*/);

    if (widget)
    {
        // deselect focus widget
        UIWidget *focus = GetFocusWidget();
        if (focus)
            focus->Deselect();

        // find the current top widget and disable it
        foreach (UIWidget* child, m_children)
        {
            if (child->IsEnabled())
                child->Deactivate();
        }

        // activate new
        widget->Activate();
        return true;
    }

    return false;
}

void UIWidget::SetPositionChanged(bool Changed)
{
    m_positionChanged = Changed;
}

void UIWidget::SetAlpha(qreal Alpha)
{
    m_effect->m_alpha = Alpha;
}

void UIWidget::SetColor(qreal Color)
{
    m_effect->m_color = Color;
}

void UIWidget::SetZoom(qreal Zoom)
{
    m_effect->m_hZoom = Zoom;
    m_effect->m_vZoom = Zoom;
}

void UIWidget::SetHorizontalZoom(qreal HorizontalZoom)
{
    m_effect->m_hZoom = HorizontalZoom;
}

void UIWidget::SetVerticalZoom(qreal VerticalZoom)
{
    m_effect->m_vZoom = VerticalZoom;
}

void UIWidget::SetRotation(qreal Rotate)
{
    m_effect->m_rotation = Rotate;
}

void UIWidget::SetCentre(int Centre)
{
    m_effect->m_centre = (UIEffect::Centre)Centre;
}

void UIWidget::SetHorizontalReflection(qreal Reflect)
{
    m_effect->m_hReflecting = true;
    m_effect->m_hReflection = Reflect * m_rootParent->GetYScale();
}

void UIWidget::SetVerticalReflection(qreal Reflect)
{
    m_effect->m_vReflecting = true;
    m_effect->m_vReflection = Reflect * m_rootParent->GetXScale();
}

void UIWidget::SetHorizontalReflecting(bool Reflecting)
{
    m_effect->m_hReflecting = Reflecting;
}

void UIWidget::SetVerticalReflecting(bool Reflecting)
{
    m_effect->m_vReflecting = Reflecting;
}

void UIWidget::SetScaledPosition(const QPointF &Position)
{
    m_unscaledRect.moveTop(Position.x() / m_rootParent->GetXScale());
    m_unscaledRect.moveLeft(Position.y() / m_rootParent->GetYScale());
    m_scaledRect.moveTopLeft(Position);
    SetPositionChanged(true);
}

void UIWidget::SetPosition(QPointF Position)
{
    m_unscaledRect.moveTopLeft(Position);
    m_scaledRect.moveTopLeft(ScalePointF(Position));
    SetPositionChanged(true);
}

void UIWidget::SetPosition(qreal Left, qreal Top)
{
    SetPosition(QPointF(Left, Top));
}

void UIWidget::SetSize(QSizeF Size)
{
    m_unscaledRect.setSize(Size);
    m_scaledRect.setSize(ScaleSizeF(Size));
    SetPositionChanged(true);
}

void UIWidget::SetSize(qreal Width, qreal Height)
{
    SetSize(QSizeF(Width, Height));
}

void UIWidget::SetClippingRect(int Left, int Top, int Width, int Height)
{
    QRectF rect(Left, Top, Width, Height);
    QRectF scaled = m_rootParent->ScaleRect(rect);
    m_clipping = true;
    m_clipRect = QRect(scaled.left(), scaled.top(), scaled.width(), scaled.height());
}

WidgetFactory* WidgetFactory::gWidgetFactory = NULL;

WidgetFactory::WidgetFactory()
{
    nextWidgetFactory = gWidgetFactory;
    gWidgetFactory = this;
}

WidgetFactory::~WidgetFactory()
{
}

WidgetFactory* WidgetFactory::GetWidgetFactory(void)
{
    return gWidgetFactory;
}

WidgetFactory* WidgetFactory::NextFactory(void) const
{
    return nextWidgetFactory;
}

static class UIWidgetFactory : public WidgetFactory
{
    void RegisterConstructor(QScriptEngine *Engine)
    {
        if (!Engine)
            return;

        QScriptValue constructor = Engine->newFunction(UIWidgetConstructor);
        Engine->globalObject().setProperty("UIWidget", constructor);
    }

    UIWidget* Create(const QString &Type, UIWidget *Root, UIWidget *Parent,
                     const QString &Name, int Flags)
    {
        if (Type == "widget" || Type == "window")
            return new UIWidget(Root, Parent, Name, Flags);
        return NULL;
    }

} UIWidgetFactory;

