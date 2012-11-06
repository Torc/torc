/* Class TorcXMLSerialiser
*
* This file is part of the Torc project.
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
#include "torcxmlserialiser.h"

TorcXMLSerialiser::TorcXMLSerialiser()
  : TorcSerialiser(),
    m_xmlStream(NULL)
{
}

TorcXMLSerialiser::~TorcXMLSerialiser()
{
    delete m_xmlStream;
}

HTTPResponseType TorcXMLSerialiser::ResponseType(void)
{
    return HTTPResponseXML;
}

void TorcXMLSerialiser::Prepare(void)
{
    if (!m_xmlStream)
        m_xmlStream = new QXmlStreamWriter(m_content);
}

void TorcXMLSerialiser::Begin(void)
{
    m_xmlStream->writeStartDocument("1.0");
}

void TorcXMLSerialiser::End(void)
{
    m_xmlStream->writeEndDocument();
}

void TorcXMLSerialiser::AddProperty(const QString &Name, const QVariant &Value)
{
    QString name = ContentName(Name);
    m_xmlStream->writeStartElement(name);
    VariantToXML(name, Value);
    m_xmlStream->writeEndElement();
}

QString TorcXMLSerialiser::ContentName(const QString &Name)
{
    if (Name.startsWith("Q"))
        return Name.mid(1);
    return Name;
}

void TorcXMLSerialiser::VariantToXML(const QString &Name, const QVariant &Value)
{
    switch(Value.type())
    {
        case QMetaType::QVariantList: ListToXML(Name, Value.toList());             return;
        case QMetaType::QStringList:  StringListToXML(Name, Value.toStringList()); return;
        case QMetaType::QVariantMap:  MapToXML(Name, Value.toMap());               return;
        case QMetaType::QVariantHash: HashToXML(Name, Value.toHash());             return;
        case QMetaType::QDateTime:
        {
            QDateTime datetime(Value.toDateTime());
            if (datetime.isNull())
                m_xmlStream->writeAttribute("xsi:nil", "true");
            m_xmlStream->writeCharacters(datetime.toString(Qt::ISODate));
            return;
        }
        default: m_xmlStream->writeCharacters(Value.toString()); return;
    }
}

void TorcXMLSerialiser::ListToXML(const QString &Name, const QVariantList &Value)
{
    if (!Value.isEmpty())
    {
        int type = Value[0].type();
        QVariantList::const_iterator it = Value.begin();
        for ( ; it != Value.end(); ++it)
        {
            if ((int)(*it).type() != type)
            {
                VariantToXML("Error", QVARIANT_ERROR);
                return;
            }
        }
    }

    QVariantList::const_iterator it = Value.begin();
    for ( ; it != Value.end(); ++it)
    {
        m_xmlStream->writeStartElement(Name);
        VariantToXML(Name, (*it));
        m_xmlStream->writeEndElement();
    }
}

void TorcXMLSerialiser::StringListToXML(const QString &Name, const QStringList &Value)
{
    QStringList::const_iterator it = Value.begin();
    for ( ; it != Value.end(); ++it)
    {
        m_xmlStream->writeStartElement("String");
        m_xmlStream->writeCharacters((*it));
        m_xmlStream->writeEndElement();
    }
}

void TorcXMLSerialiser::MapToXML(const QString &Name, const QVariantMap &Value)
{
    QString name = ContentName(Name);

    QVariantMap::const_iterator it = Value.begin();
    for ( ; it != Value.end(); ++it)
    {
        m_xmlStream->writeStartElement(name);
        m_xmlStream->writeStartElement("Key");
        m_xmlStream->writeCharacters(it.key());
        m_xmlStream->writeEndElement();
        m_xmlStream->writeStartElement("Value");
        VariantToXML(name, it.value());
        m_xmlStream->writeEndElement();
        m_xmlStream->writeEndElement();
    }
}

void TorcXMLSerialiser::HashToXML(const QString &Name, const QVariantHash &Value)
{
    QString name = ContentName(Name);

    QVariantHash::const_iterator it = Value.begin();
    for ( ; it != Value.end(); ++it)
    {
        m_xmlStream->writeStartElement(name);
        m_xmlStream->writeStartElement("Key");
        m_xmlStream->writeCharacters(it.key());
        m_xmlStream->writeEndElement();
        m_xmlStream->writeStartElement("Value");
        VariantToXML(name, it.value());
        m_xmlStream->writeEndElement();
        m_xmlStream->writeEndElement();
    }
}
