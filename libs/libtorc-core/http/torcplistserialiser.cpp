/* Class TorcPListSerialiser
*
* Copyright (C) Mark Kendall 2012-13
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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

// Torc
#include "torcplistserialiser.h"

TorcPListSerialiser::TorcPListSerialiser()
  : TorcXMLSerialiser()
{
}

TorcPListSerialiser::~TorcPListSerialiser()
{
}

HTTPResponseType TorcPListSerialiser::ResponseType(void)
{
    return HTTPResponsePList;
}

void TorcPListSerialiser::Begin(void)
{
    m_xmlStream->setAutoFormatting(true);
    m_xmlStream->setAutoFormattingIndent(4);
    m_xmlStream->writeStartDocument("1.0");
    m_xmlStream->writeDTD("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">");
    m_xmlStream->writeStartElement("plist");
    m_xmlStream->writeAttribute("version", "1.0");
    m_xmlStream->writeStartElement("dict");
}

void TorcPListSerialiser::AddProperty(const QString &Name, const QVariant &Value)
{
    PListFromVariant(Name, Value);
}

void TorcPListSerialiser::End(void)
{
    m_xmlStream->writeEndElement();
    m_xmlStream->writeEndElement();
    m_xmlStream->writeEndDocument();
}

void TorcPListSerialiser::PListFromVariant(const QString &Name, const QVariant &Value, bool NeedKey)
{
    switch ((int)Value.type())
    {
        case QMetaType::QVariantList: PListFromList(Name, Value.toList());             break;
        case QMetaType::QStringList:  PListFromStringList(Name, Value.toStringList()); break;
        case QMetaType::QVariantMap:  PListFromMap(Name, Value.toMap());               break;
        case QMetaType::QDateTime:
        {
            if (Value.toDateTime().isValid())
            {
                if (NeedKey)
                    m_xmlStream->writeTextElement("key", Name);
                m_xmlStream->writeTextElement("date", Value.toDateTime().toUTC().toString("yyyy-MM-ddThh:mm:ssZ"));
            }
            break;
        }
        case QMetaType::QByteArray:
        {
            if (!Value.toByteArray().isNull())
            {
                if (NeedKey)
                    m_xmlStream->writeTextElement("key", Name);
                m_xmlStream->writeTextElement("data", Value.toByteArray().toBase64().data());
            }
            break;
        }
        case QMetaType::Bool:
        {
            if (NeedKey)
                m_xmlStream->writeTextElement("key", Name);
            m_xmlStream->writeEmptyElement(Value.toBool() ? "true" : "false");
            break;
        }

        case QMetaType::UInt:
        case QMetaType::UShort:
        case QMetaType::ULong:
        case QMetaType::ULongLong:
        {
            if (NeedKey)
                m_xmlStream->writeTextElement("key", Name);
            m_xmlStream->writeTextElement("integer", QString::number(Value.toULongLong()));
            break;
        }

        case QMetaType::Int:
        case QMetaType::Short:
        case QMetaType::Long:
        case QMetaType::LongLong:
        case QMetaType::Float:
        case QMetaType::Double:
        {
            if (NeedKey)
                m_xmlStream->writeTextElement("key", Name);
            m_xmlStream->writeTextElement("real", QString("%1").arg(Value.toDouble(), 0, 'f', 6));
            break;
        }

        case QMetaType::QString:
        default:
        {
            if (NeedKey)
                m_xmlStream->writeTextElement("key", Name);
            m_xmlStream->writeTextElement("string", Value.toString());
            break;
        }
    }
}

void TorcPListSerialiser::PListFromList(const QString &Name, const QVariantList &Value)
{
    if (!Value.isEmpty())
    {
        int type = Value[0].type();

        QVariantList::const_iterator it = Value.begin();
        for ( ; it != Value.end(); ++it)
        {
            if ((int)(*it).type() != type)
            {
                PListFromVariant("Error", QVARIANT_ERROR);
                return;
            }
        }
    }

    m_xmlStream->writeTextElement("key", Name);
    m_xmlStream->writeStartElement("array");

    QVariantList::const_iterator it = Value.begin();
    for ( ; it != Value.end(); ++it)
        PListFromVariant(Name, (*it), false);

    m_xmlStream->writeEndElement();
}

void TorcPListSerialiser::PListFromStringList(const QString &Name, const QStringList &Value)
{
    m_xmlStream->writeTextElement("key", Name);
    m_xmlStream->writeStartElement("array");

    QStringList::const_iterator it = Value.begin();
    for ( ; it != Value.end(); ++it)
        m_xmlStream->writeTextElement("string", (*it));

    m_xmlStream->writeEndElement();
}

void TorcPListSerialiser::PListFromMap(const QString &Name, const QVariantMap &Value)
{
    m_xmlStream->writeTextElement("key", Name);
    m_xmlStream->writeStartElement("dict");

    QVariantMap::const_iterator it = Value.begin();
    for ( ; it != Value.end(); ++it)
        PListFromVariant(it.key(), it.value());

    m_xmlStream->writeEndElement();
}

class TorcApplePListSerialiserFactory : public TorcSerialiserFactory
{
  public:
    TorcApplePListSerialiserFactory() : TorcSerialiserFactory("text/x-apple-plist+xml", "PList")
    {
    }

    TorcSerialiser* Create(void)
    {
        return new TorcPListSerialiser();
    }
} TorcApplePListSerialiserFactory;

class TorcPListSerialiserFactory : public TorcSerialiserFactory
{
  public:
    TorcPListSerialiserFactory() : TorcSerialiserFactory("application/plist", "PList")
    {
    }

    TorcSerialiser* Create(void)
    {
        return new TorcPListSerialiser();
    }
} TorcPListSerialiserFactory;


