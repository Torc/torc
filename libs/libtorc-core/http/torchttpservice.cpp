/* Class TorcHTTPService
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
#include <QCoreApplication>
#include <QObject>
#include <QMetaType>
#include <QMetaMethod>
#include <QTime>
#include <QDate>
#include <QDateTime>

// Torc
#include "torclogging.h"
#include "torchttpconnection.h"
#include "torchttpserver.h"
#include "torchttprequest.h"
#include "torcserialiser.h"
#include "torchttpservice.h"

class MethodParameters
{
  public:
    MethodParameters(int Index, const QMetaMethod &Method)
      : m_index(Index)
    {
        // the return type/value is first
        int returntype = QMetaType::type(Method.typeName());

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
        void* param = returntype > 0 ? QMetaType::construct(returntype) : NULL;
#else
        void* param = returntype > 0 ? QMetaType::create(returntype) : NULL;
#endif

        m_parameters.append(param);
        m_types.append(returntype > 0 ? returntype : 0);
        m_names.append("");

        QList<QByteArray> names = Method.parameterNames();
        QList<QByteArray> types = Method.parameterTypes();

        // add type/value for each method parameter
        for (int i = 0; i < names.size(); ++i)
        {
            int type    = QMetaType::type(types[i]);

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
            void* param = type != 0 ? QMetaType::construct(type) : NULL;
#else
            void* param = type != 0 ? QMetaType::create(type) : NULL;
#endif
            m_names.append(names[i]);
            m_parameters.append(param);
            m_types.append(type);
        }
    }

    ~MethodParameters()
    {
        for (int i = 0; i < m_parameters.size(); ++i)
            if (m_parameters.data()[i])
                QMetaType::destroy(m_types.data()[i], m_parameters.data()[i]);
    }

    QVariant Invoke(QObject *Object, TorcHTTPRequest *Request)
    {
        QMap<QString,QString> queries = Request->Queries();
        for (int i = 1; i < m_names.size(); ++i)
            SetValue(m_parameters[i], queries.value(m_names[i]), m_types[i]);

        Object->qt_metacall(QMetaObject::InvokeMetaMethod, m_index, m_parameters.data());

        return QVariant(m_types[0], m_parameters[0]);
    }

    void SetValue(void* Pointer, const QString &Value, int Type)
    {
        if (!Pointer)
            return;

        switch (Type)
        {
            case QMetaType::Char:       *((char*)Pointer)          = Value.isEmpty() ? 0 : Value.at(0).toLatin1(); return;
            case QMetaType::UChar:      *((unsigned char*)Pointer) = Value.isEmpty() ? 0 :Value.at(0).toLatin1();  return;
            case QMetaType::QChar:      *((QChar*)Pointer)         = Value.isEmpty() ? 0 : Value.at(0);            return;
            case QMetaType::Bool:       *((bool*)Pointer)          = ToBool(Value);       return;
            case QMetaType::Short:      *((short*)Pointer)         = Value.toShort();     return;
            case QMetaType::UShort:     *((ushort*)Pointer)        = Value.toUShort();    return;
            case QMetaType::Int:        *((int*)Pointer)           = Value.toInt();       return;
            case QMetaType::UInt:       *((uint*)Pointer)          = Value.toUInt();      return;
            case QMetaType::Long:       *((long*)Pointer)          = Value.toLong();      return;
            case QMetaType::ULong:      *((ulong*)Pointer)         = Value.toULong();     return;
            case QMetaType::LongLong:   *((qlonglong*)Pointer)     = Value.toLongLong();  return;
            case QMetaType::ULongLong:  *((qulonglong*)Pointer)    = Value.toULongLong(); return;
            case QMetaType::Double:     *((double*)Pointer)        = Value.toDouble();    return;
            case QMetaType::Float:      *((float*)Pointer)         = Value.toFloat();     return;
            case QMetaType::QString:    *((QString*)Pointer)       = Value;               return;
            case QMetaType::QByteArray: *((QByteArray*)Pointer)    = Value.toUtf8();      return;
            case QMetaType::QTime:      *((QTime*)Pointer)         = QTime::fromString(Value, Qt::ISODate); return;
            case QMetaType::QDate:      *((QDate*)Pointer)         = QDate::fromString(Value, Qt::ISODate); return;
            case QMetaType::QDateTime:
            {
                QDateTime dt = QDateTime::fromString(Value, Qt::ISODate);
                dt.setTimeSpec(Qt::UTC);
                *((QDateTime*)Pointer) = dt;
                return;
            }
            default: break;
        }
    }

    bool ToBool(const QString &Value)
    {
        if (Value.compare("1", Qt::CaseInsensitive) == 0)
            return true;
        if (Value.compare("true", Qt::CaseInsensitive) == 0)
            return true;
        if (Value.compare("y", Qt::CaseInsensitive) == 0)
            return true;
        return false;
    }

    int                 m_index;
    QVector<QByteArray> m_names;
    QVector<void*>      m_parameters;
    QVector<int>        m_types;
};

TorcHTTPService::TorcHTTPService(QObject *Parent, const QString &Signature, const QString &Name,
                                 const QMetaObject &MetaObject, const QString &Blacklist)
  : TorcHTTPHandler(SERVICES_DIRECTORY + Signature, Name),
    m_parent(Parent),
    m_metaObject(MetaObject)
{
    QStringList blacklist = Blacklist.split(",");

    for (int i = 0; i < m_metaObject.methodCount(); ++i)
    {
        QMetaMethod method = m_metaObject.method(i);

        if ((method.methodType() == QMetaMethod::Slot) &&
            (method.access()     == QMetaMethod::Public))
        {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
            QString name(method.signature());
#else
            QString name(method.methodSignature());
#endif
            name = name.section('(', 0, 0);

            if (name == "deleteLater" || blacklist.contains(name))
                continue;

            m_methods.insert(name, new MethodParameters(i, method));
        }
    }

    TorcHTTPServer::RegisterHandler(this);
}

TorcHTTPService::~TorcHTTPService()
{
    TorcHTTPServer::DeregisterHandler(this);

    QMap<QString,MethodParameters*>::iterator it = m_methods.begin();
    for ( ; it != m_methods.end(); ++it)
        delete (*it);
}

void TorcHTTPService::ProcessHTTPRequest(TorcHTTPServer *Server, TorcHTTPRequest *Request, TorcHTTPConnection *Connection)
{
    QString method = Request->GetMethod();

    if (method.compare("help", Qt::CaseInsensitive) == 0)
    {
        UserHelp(Server, Request, Connection);
        return;
    }

    QMap<QString,MethodParameters*>::iterator it = m_methods.find(method);
    if (it != m_methods.end())
    {
        QVariant result = (*it)->Invoke(m_parent, Request);
        Request->SetStatus(HTTP_OK);
        TorcSerialiser *serialiser = Request->GetSerialiser();
        Request->SetResponseType(serialiser->ResponseType());
        Request->SetResponseContent(serialiser->Serialise(result));
        delete serialiser;
    }
    else
    {
        Request->SetStatus(HTTP_NotFound);
        Request->SetResponseType(HTTPResponseDefault);
    }
}

void TorcHTTPService::UserHelp(TorcHTTPServer *Server, TorcHTTPRequest *Request, TorcHTTPConnection *Connection)
{
    if (!Request || !Server)
        return;

    QByteArray *result = new QByteArray(1024, 0);
    QTextStream stream(result);

    stream << "<html><head><title>" << QCoreApplication::applicationName() << "</title></head>";
    stream << "<body><h1><a href='/'>" << QCoreApplication::applicationName() << "</a> ";
    stream << "<a href='" << SERVICES_DIRECTORY << "/'>" << QObject::tr("Services") << "</a> " << m_name << "</h1>";

    if (m_methods.isEmpty())
    {
        stream << "<h3>" << QObject::tr("This service has no publicly available methods") << "</h3>";
    }
    else
    {
        stream << "<h3>" << QObject::tr("Method list for ") << m_signature << "</h3>";

        int count   = 0;

        QMap<QString,MethodParameters*>::const_iterator it = m_methods.begin();
        QMap<QString,MethodParameters*>::const_iterator example = it;
        for ( ; it != m_methods.end(); ++it)
        {
            MethodParameters *params = it.value();
            int size = params->m_types.size();
            if (size > count)
            {
                example = it;
                count = size;
            }

            QString method = QString("%1 %2(").arg(QMetaType::typeName(params->m_types[0])).arg(it.key());

            bool first = true;
            for (int i = 1; i < size; ++i)
            {
                if (!first)
                    method += ", ";
                method += QString("%1 %2").arg(QMetaType::typeName(params->m_types[i])).arg(params->m_names[i].data());
                first = false;
            }

            stream << method << ")<br>";
        }

        QString url = QString("http://%1:%2").arg(Connection->Socket()->localAddress().toString()).arg(Connection->Socket()->localPort());
        QString usage = QString("%1%2%3").arg(url).arg(m_signature).arg(example.key());

        if (example.value()->m_types.size() > 1)
        {
            usage += "?";
            for (int i = 1; i < example.value()->m_types.size(); ++i)
                usage += QString("%1%2=Value%3").arg(i == 1 ? "" : "&").arg(example.value()->m_names[i].data()).arg(i);
        }
        stream << "<p><h3>" << QObject::tr("Example usage:") << "</h3><p>" << usage;
    }

    stream << "</body></html>";
    stream.flush();
    Request->SetStatus(HTTP_OK);
    Request->SetResponseType(HTTPResponseHTML);
    Request->SetResponseContent(result);
}
