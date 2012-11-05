/* Class TorcPList
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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

/**
 * \class TorcPList
 *  A parser for binary property lists, using QVariant for internal
 *  storage. Values can be queried using GetValue and the structure
 *  can be exported to Xml with ToXml().
 *
 *  Support for importing Xml formatted property lists will be added.
 */

// Qt
#include <QtEndian>
#include <QDateTime>
#include <QTextStream>
#include <QTextCodec>
#include <QBuffer>

// Torc
#include "torclogging.h"
#include "torcplist.h"

#define MAGIC   QByteArray("bplist")
#define VERSION QByteArray("00")
#define MAGIC_SIZE   6
#define VERSION_SIZE 2
#define TRAILER_SIZE 26
#define MIN_SIZE (MAGIC_SIZE + VERSION_SIZE + TRAILER_SIZE)
#define TRAILER_OFFSIZE_INDEX  0
#define TRAILER_PARMSIZE_INDEX 1
#define TRAILER_NUMOBJ_INDEX   2
#define TRAILER_ROOTOBJ_INDEX  10
#define TRAILER_OFFTAB_INDEX   18

enum
{
    BPLIST_NULL    = 0x00,
    BPLIST_FALSE   = 0x08,
    BPLIST_TRUE    = 0x09,
    BPLIST_FILL    = 0x0F,
    BPLIST_UINT    = 0x10,
    BPLIST_REAL    = 0x20,
    BPLIST_DATE    = 0x30,
    BPLIST_DATA    = 0x40,
    BPLIST_STRING  = 0x50,
    BPLIST_UNICODE = 0x60,
    BPLIST_UID     = 0x70,
    BPLIST_ARRAY   = 0xA0,
    BPLIST_SET     = 0xC0,
    BPLIST_DICT    = 0xD0
};

// NB Do not call this twice on the same data
static void convert_float(quint8 *p, quint8 s)
{
#if HAVE_BIGENDIAN && !defined (__VFP_FP__)
    return;
#else
    for (quint8 i = 0; i < (s / 2); i++)
    {
        quint8 t = p[i];
        quint8 j = ((s - 1) + 0) - i;
        p[i] = p[j];
        p[j] = t;
    }
#endif
}

TorcPList::TorcPList(const QByteArray &Data)
  : m_data(NULL),
    m_offsetTable(NULL),
    m_rootObj(0),
    m_numObjs(0),
    m_offsetSize(0),
    m_parmSize(0),
    m_codec(QTextCodec::codecForName("UTF-16BE"))
{
    ParseBinaryPList(Data);
}

QVariant TorcPList::GetValue(const QString &Key)
{
    if ((int)m_result.type() != QMetaType::QVariantMap)
        return QVariant();

    QVariantMap map = m_result.toMap();
    QMapIterator<QString,QVariant> it(map);
    while (it.hasNext())
    {
        it.next();
        if (Key == it.key())
            return it.value();
    }
    return QVariant();
}

QString TorcPList::ToString(void)
{
    QByteArray res;
    QBuffer buf(&res);
    buf.open(QBuffer::WriteOnly);
    if (!ToXML(&buf))
        return QString("");
    return QString::fromUtf8(res.data());
}

bool TorcPList::ToXML(QIODevice *Device)
{
    QXmlStreamWriter XML(Device);
    XML.setAutoFormatting(true);
    XML.setAutoFormattingIndent(4);
    XML.writeStartDocument();
    XML.writeDTD("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">");
    XML.writeStartElement("plist");
    XML.writeAttribute("version", "1.0");
    bool success = ToXML(m_result, XML);
    XML.writeEndElement();
    XML.writeEndDocument();
    if (!success)
        LOG(VB_GENERAL, LOG_WARNING, "Invalid result.");
    return success;
}

bool TorcPList::ToXML(const QVariant &Data, QXmlStreamWriter &XML)
{
    switch (Data.type())
    {
        case QMetaType::QVariantMap:
            DictToXML(Data, XML);
            break;
        case QMetaType::QVariantList:
            ArrayToXML(Data, XML);
            break;
        case QMetaType::Double:
            XML.writeTextElement("real",
                                 QString("%1").arg(Data.toDouble(), 0, 'f', 6));
            break;
        case QMetaType::QByteArray:
            XML.writeTextElement("data",
                                 Data.toByteArray().toBase64().data());
            break;
        case QMetaType::ULongLong:
            XML.writeTextElement("integer",
                                 QString("%1").arg(Data.toULongLong()));
            break;
        case QMetaType::QString:
            XML.writeTextElement("string", Data.toString());
            break;
        case QMetaType::QDateTime:
            XML.writeTextElement("date", Data.toDateTime().toString(Qt::ISODate));
            break;
        case QMetaType::Bool:
            {
                bool val = Data.toBool();
                XML.writeEmptyElement(val ? "true" : "false");
            }
            break;
        default:
            LOG(VB_GENERAL, LOG_WARNING, "Unknown type.");
            return false;
    }
    return true;
}

void TorcPList::DictToXML(const QVariant &Data, QXmlStreamWriter &XML)
{
    XML.writeStartElement("dict");

    QVariantMap map = Data.toMap();
    QMapIterator<QString,QVariant> it(map);
    while (it.hasNext())
    {
        it.next();
        XML.writeTextElement("key", it.key());
        ToXML(it.value(), XML);
    }

    XML.writeEndElement();
}

void TorcPList::ArrayToXML(const QVariant &Data, QXmlStreamWriter &XML)
{
    XML.writeStartElement("array");

    QList<QVariant> list = Data.toList();
    foreach (QVariant item, list)
        ToXML(item, XML);

    XML.writeEndElement();
}

void TorcPList::ParseBinaryPList(const QByteArray &Data)
{
    // reset
    m_result = QVariant();

    // check minimum size
    quint32 size = Data.size();
    if (size < MIN_SIZE)
        return;

    LOG(VB_GENERAL, LOG_DEBUG, QString("Binary: size %1, startswith '%2'")
        .arg(size).arg(Data.left(8).data()));

    // check plist type & version
    if ((Data.left(6) != MAGIC) ||
        (Data.mid(MAGIC_SIZE, VERSION_SIZE) != VERSION))
    {
        LOG(VB_GENERAL, LOG_ERR, "Unrecognised start sequence. Corrupt?");
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Parsing binary plist (%1 bytes)")
        .arg(size));

    m_data = (quint8*)Data.data();
    quint8* trailer = m_data + size - TRAILER_SIZE;
    m_offsetSize   = *(trailer + TRAILER_OFFSIZE_INDEX);
    m_parmSize = *(trailer + TRAILER_PARMSIZE_INDEX);
    m_numObjs  = qToBigEndian(*((quint64*)(trailer + TRAILER_NUMOBJ_INDEX)));
    m_rootObj  = qToBigEndian(*((quint64*)(trailer + TRAILER_ROOTOBJ_INDEX)));
    quint64 offset_tindex = qToBigEndian(*((quint64*)(trailer + TRAILER_OFFTAB_INDEX)));
    m_offsetTable = m_data + offset_tindex;

    LOG(VB_GENERAL, LOG_DEBUG,
        QString("numObjs: %1 parmSize: %2 offsetSize: %3 rootObj: %4 offsetindex: %5")
            .arg(m_numObjs).arg(m_parmSize).arg(m_offsetSize).arg(m_rootObj).arg(offset_tindex));

    // something wrong?
    if (!m_numObjs || !m_parmSize || !m_offsetSize)
    {
        LOG(VB_GENERAL, LOG_ERR, "Error parsing binary plist. Corrupt?");
        return;
    }

    // parse
    m_result = ParseBinaryNode(m_rootObj);

    LOG(VB_GENERAL, LOG_INFO, "Parse complete.");
}

QVariant TorcPList::ParseBinaryNode(quint64 Num)
{
    quint8* data = GetBinaryObject(Num);
    if (!data)
        return QVariant();

    quint16 type = (*data) & 0xf0;
    quint64 size = (*data) & 0x0f;

    switch (type)
    {
        case BPLIST_SET:
        case BPLIST_ARRAY:   return ParseBinaryArray(data);
        case BPLIST_DICT:    return ParseBinaryDict(data);
        case BPLIST_STRING:  return ParseBinaryString(data);
        case BPLIST_UINT:    return ParseBinaryUInt(&data);
        case BPLIST_REAL:    return ParseBinaryReal(data);
        case BPLIST_DATE:    return ParseBinaryDate(data);
        case BPLIST_DATA:    return ParseBinaryData(data);
        case BPLIST_UNICODE: return ParseBinaryUnicode(data);
        case BPLIST_NULL:
        {
            switch (size)
            {
                case BPLIST_TRUE:  return QVariant(true);
                case BPLIST_FALSE: return QVariant(false);
                case BPLIST_NULL:
                default:           return QVariant();
            }
        }
        case BPLIST_UID: // FIXME
        default: return QVariant();
    }

    return QVariant();
}

quint64 TorcPList::GetBinaryUInt(quint8 *Data, quint64 Size)
{
    if (Size == 1) return (quint64)(*(Data));
    if (Size == 2) return (quint64)(qToBigEndian(*((quint16*)Data)));
    if (Size == 4) return (quint64)(qToBigEndian(*((quint32*)Data)));
    if (Size == 8) return (quint64)(qToBigEndian(*((quint64*)Data)));

    if (Size == 3)
    {
#if HAVE_BIGENDIAN
        return (quint64)(((*Data) << 16) + (*(Data + 1) << 8) + (*(Data + 2)));
#else
        return (quint64)((*Data) + (*(Data + 1) << 8) + ((*(Data + 2)) << 16));
#endif
    }

    return 0;
}

quint8* TorcPList::GetBinaryObject(quint64 Num)
{
    if (Num > m_numObjs)
        return NULL;

    quint8* p = m_offsetTable + (Num * m_offsetSize);
    quint64 offset = GetBinaryUInt(p, m_offsetSize);
    LOG(VB_GENERAL, LOG_DEBUG, QString("GetBinaryObject Num %1, offsize %2 offset %3")
        .arg(Num).arg(m_offsetSize).arg(offset));

    return m_data + offset;
}

QVariantMap TorcPList::ParseBinaryDict(quint8 *Data)
{
    QVariantMap result;
    if (((*Data) & 0xf0) != BPLIST_DICT)
        return result;

    quint64 count = GetBinaryCount(&Data);

    LOG(VB_GENERAL, LOG_DEBUG, QString("Dict: Size %1").arg(count));

    if (!count)
        return result;

    quint64 off = m_parmSize * count;
    for (quint64 i = 0; i < count; i++, Data += m_parmSize)
    {
        quint64 keyobj = GetBinaryUInt(Data, m_parmSize);
        quint64 valobj = GetBinaryUInt(Data + off, m_parmSize);
        QVariant key = ParseBinaryNode(keyobj);
        QVariant val = ParseBinaryNode(valobj);
        if (!key.canConvert<QString>())
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Invalid dictionary key type '%1' (key %2)")
                .arg(key.type()).arg(keyobj));
            return result;
        }

        result.insertMulti(key.toString(), val);
    }

    return result;
}

QList<QVariant> TorcPList::ParseBinaryArray(quint8 *Data)
{
    QList<QVariant> result;
    if (((*Data) & 0xf0) != BPLIST_ARRAY)
        return result;

    quint64 count = GetBinaryCount(&Data);

    LOG(VB_GENERAL, LOG_DEBUG, QString("Array: Size %1").arg(count));

    if (!count)
        return result;

    for (quint64 i = 0; i < count; i++, Data += m_parmSize)
    {
        quint64 obj = GetBinaryUInt(Data, m_parmSize);
        QVariant val = ParseBinaryNode(obj);
        result.push_back(val);
    }
    return result;
}

QVariant TorcPList::ParseBinaryUInt(quint8 **Data)
{
    quint64 result = 0;
    if (((**Data) & 0xf0) != BPLIST_UINT)
        return QVariant(result);

    quint64 size = 1 << ((**Data) & 0x0f);
    (*Data)++;
    result = GetBinaryUInt(*Data, size);
    (*Data) += size;

    LOG(VB_GENERAL, LOG_DEBUG, QString("UInt: %1").arg(result));
    return QVariant(result);
}

QVariant TorcPList::ParseBinaryString(quint8 *Data)
{
    QString result;
    if (((*Data) & 0xf0) != BPLIST_STRING)
        return result;

    quint64 count = GetBinaryCount(&Data);
    if (!count)
        return result;

    result = QString::fromLatin1((const char*)Data, count);
    LOG(VB_GENERAL, LOG_DEBUG, QString("ASCII String: %1").arg(result));
    return QVariant(result);
}

QVariant TorcPList::ParseBinaryReal(quint8 *Data)
{
    double result = 0.0;
    if (((*Data) & 0xf0) != BPLIST_REAL)
        return result;

    quint64 count = GetBinaryCount(&Data);
    if (!count)
        return result;

    count = (quint64)(1 << count);
    if (count == sizeof(float))
    {
        convert_float(Data, count);
        result = (double)(*((float*)Data));
    }
    else if (count == sizeof(double))
    {
        convert_float(Data, count);
        result = *((double*)Data);
    }

    LOG(VB_GENERAL, LOG_DEBUG, QString("Real: %1").arg(result, 0, 'f', 6));
    return QVariant(result);
}

QVariant TorcPList::ParseBinaryDate(quint8 *Data)
{
    QDateTime result;
    if (((*Data) & 0xf0) != BPLIST_DATE)
        return result;

    quint64 count = GetBinaryCount(&Data);
    if (count != 3)
        return result;

    convert_float(Data, 8);
    result = QDateTime::fromTime_t((quint64)(*((double*)Data)));
    LOG(VB_GENERAL, LOG_DEBUG, QString("Date: %1").arg(result.toString(Qt::ISODate)));
    return QVariant(result);
}

QVariant TorcPList::ParseBinaryData(quint8 *Data)
{
    QByteArray result;
    if (((*Data) & 0xf0) != BPLIST_DATA)
        return result;

    quint64 count = GetBinaryCount(&Data);
    if (!count)
        return result;

    result = QByteArray((const char*)Data, count);
    LOG(VB_GENERAL, LOG_DEBUG, QString("Data: Size %1 (count %2)")
        .arg(result.size()).arg(count));
    return QVariant(result);
}

QVariant TorcPList::ParseBinaryUnicode(quint8 *Data)
{
    QString result;
    if (((*Data) & 0xf0) != BPLIST_UNICODE)
        return result;

    quint64 count = GetBinaryCount(&Data);
    if (!count)
        return result;

    return QVariant(m_codec->toUnicode((char*)Data, count << 1));
}

quint64 TorcPList::GetBinaryCount(quint8 **Data)
{
    quint64 count = (**Data) & 0x0f;
    (*Data)++;
    if (count == 0x0f)
    {
        QVariant newcount = ParseBinaryUInt(Data);
        if (!newcount.canConvert<quint64>())
            return 0;
        count = newcount.toULongLong();
    }
    return count;
}
