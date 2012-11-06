/* Class TorcJSONSerialiser
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
#include "torcjsonserialiser.h"

TorcJSONSerialiser::TorcJSONSerialiser(bool Javascript)
  : TorcSerialiser(),
    m_javaScriptType(Javascript),
    m_textStream(NULL),
    m_needComma(false)
{
}

TorcJSONSerialiser::~TorcJSONSerialiser()
{
    delete m_textStream;
}

HTTPResponseType TorcJSONSerialiser::ResponseType(void)
{
    return m_javaScriptType ? HTTPResponseJSONJavascript : HTTPResponseJSON;
}

void TorcJSONSerialiser::Prepare(void)
{
    if (!m_textStream)
        m_textStream = new QTextStream(m_content);
}

void TorcJSONSerialiser::Begin(void)
{
    m_needComma = false;
    *m_textStream << "{";
}

void TorcJSONSerialiser::AddProperty(const QString &Name, const QVariant &Value)
{
    if (m_needComma)
        *m_textStream << ", ";

    *m_textStream << "\"" << Name << "\": ";
    JSONfromVariant(Value);
    m_needComma = true;
}

void TorcJSONSerialiser::End(void)
{
    m_needComma = false;
    *m_textStream << "}";
    m_textStream->flush();
}

void TorcJSONSerialiser::JSONfromVariant(const QVariant &Value)
{
    switch(Value.type())
    {
        case QMetaType::QVariantList: JSONfromList(Value.toList());             return;
        case QMetaType::QStringList:  JSONfromStringList(Value.toStringList()); return;
        case QMetaType::QVariantMap:  JSONfromMap(Value.toMap());               return;
        case QMetaType::QVariantHash: JSONfromHash(Value.toHash());             return;
        case QMetaType::QDateTime:
        {
            QDateTime datetime(Value.toDateTime());
            *m_textStream << "\"" << Encode(datetime.toString(Qt::ISODate)) << "\"";
            return;
        }
        default:
        {
            *m_textStream << "\"" << Encode(Value.toString()) << "\"";
            return;
        }
    }
}

void TorcJSONSerialiser::JSONfromList(const QVariantList &Value)
{
    if (!Value.isEmpty())
    {
        int type = Value[0].type();
        QVariantList::const_iterator it = Value.begin();
        for ( ; it != Value.end(); ++it)
        {
            if ((int)(*it).type() != type)
            {
                JSONfromVariant(QVARIANT_ERROR);
                return;
            }
        }
    }

    *m_textStream << "[";

    bool first = true;
    QVariantList::const_iterator it = Value.begin();
    for ( ; it != Value.end(); ++it)
    {
        if (first)
            first = false;
        else
            *m_textStream << ",";

        JSONfromVariant((*it));
    }

    *m_textStream << "]";
}

void TorcJSONSerialiser::JSONfromStringList(const QStringList &Value)
{
    *m_textStream << "[";

    bool first = true;
    QStringList::const_iterator it = Value.begin();
    for ( ; it != Value.end(); ++it)
    {
        if (first)
            first = false;
        else
            *m_textStream << ",";

        *m_textStream << "\"" << Encode((*it)) << "\"";
    }

    *m_textStream << "]";
}

void TorcJSONSerialiser::JSONfromMap(const QVariantMap &Value)
{
    *m_textStream << "{";

    bool first = true;
    QVariantMap::const_iterator it = Value.begin();
    for ( ; it != Value.end(); ++it)
    {
        if (first)
            first = false;
        else
            *m_textStream << ",";

        *m_textStream << "\"" << it.key() << "\":";
        *m_textStream << "\"" << Encode(it.value().toString()) << "\"";
    }

    *m_textStream << "}";
}

void TorcJSONSerialiser::JSONfromHash(const QVariantHash &Value)
{
    *m_textStream << "{";

    bool first = true;
    QVariantHash::const_iterator it = Value.begin();
    for ( ; it != Value.end(); ++it)
    {
        if (first)
            first = false;
        else
            *m_textStream << ",";

        *m_textStream << "\"" << it.key() << "\":";
        *m_textStream << "\"" << Encode(it.value().toString()) << "\"";
    }

    *m_textStream << "}";
}

QString TorcJSONSerialiser::Encode(const QString &String)
{
    if (String.isEmpty())
        return String;

    QString result = String;

    // TODO - does this always work with unicode?

    result.replace( '\\', "\\\\" ); // This must be first
    result.replace( '"' , "\\\"" );
    result.replace( '\b', "\\b"  );
    result.replace( '\f', "\\f"  );
    result.replace( '\n', "\\n"  );
    result.replace( "\r", "\\r"  );
    result.replace( "\t", "\\t"  );
    result.replace(  "/", "\\/"  );
    return result;
}
