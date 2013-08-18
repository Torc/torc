/* Class TorcHTTPService
*
* This file is part of the Torc project.
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
#include "torcserialiser.h"
#include "torchttpservice.h"

class MethodParameters
{
  public:
    MethodParameters(int Index, const QMetaMethod &Method, int AllowedRequestTypes, const QString &ReturnType)
      : m_valid(false),
        m_index(Index),
        m_allowedRequestTypes(AllowedRequestTypes),
        m_returnType(ReturnType)
    {
        // statically initialise the list of unsupported types (either non-serialisable (QHash)
        // or nonsensical (pointer types)
        static QList<int> unsupportedtypes;
        static QList<int> unsupportedparameters;
        static bool initialised = false;

        if (!initialised)
        {
            // this list is probably incomplete
            initialised = true;
            unsupportedtypes << QMetaType::UnknownType;
            unsupportedtypes << QMetaType::VoidStar << QMetaType::QObjectStar << QMetaType::QVariantHash;
            unsupportedtypes << QMetaType::QRect << QMetaType::QRectF << QMetaType::QSize << QMetaType::QSizeF << QMetaType::QLine << QMetaType::QLineF << QMetaType::QPoint << QMetaType::QPointF;

            unsupportedparameters << unsupportedtypes;
            unsupportedparameters << QMetaType::QVariantMap << QMetaType::QStringList << QMetaType::QVariantList;
        }

        // the return type/value is first
        int returntype = QMetaType::type(Method.typeName());

        // discard slots with an unsupported return type
        if (unsupportedtypes.contains(returntype))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Method '%1' has unsupported return type '%2'")
                .arg(Method.name().data()).arg(Method.typeName()));
            return;
        }

        m_types.append(returntype > 0 ? returntype : 0);
        m_names.append(Method.name());

        QList<QByteArray> names = Method.parameterNames();
        QList<QByteArray> types = Method.parameterTypes();

        if (names.size() > 10)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Method '%1' takes more than 10 parameters - ignoring").arg(Method.name().data()));
            return;
        }

        // add type/value for each method parameter
        for (int i = 0; i < names.size(); ++i)
        {
            int type = QMetaType::type(types[i]);

            // discard slots that use unsupported parameter types
            if (unsupportedparameters.contains(type))
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Method '%1' has unsupported parameter type '%2'")
                    .arg(Method.name().data()).arg(types[i].data()));
                return;
            }

            m_names.append(names[i]);
            m_types.append(type);
        }

        m_valid = true;
    }

    ~MethodParameters()
    {
    }

    /*! \brief Call the stored method with the arguments passed in via Request.
     *
     * \note Invoke is not thread safe and any method exposed using this class MUST ensure thread safety.
    */
    QVariant Invoke(QObject *Object, TorcHTTPRequest *Request, QString &ReturnType, bool &VoidResult)
    {
        // we cannot create a QVariant that is void and an invalid QVariant signals an error state,
        // so flag directly
        VoidResult = m_types[0] == QMetaType::Void;

        // this may be called by multiple threads simultaneously, so we need to create our own paramaters instance.
        // N.B. QMetaObject::invokeMethod only supports up to 10 arguments (plus a return value)
        void* parameters[11];
        memset(parameters, 0, 11 * sizeof(void*));
        QMap<QString,QString> queries = Request->Queries();
        int size = qMin(11, m_types.size());

        // check parameter count
        if (queries.size() != size - 1)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Method '%1' expects %2 parameters, sent %3")
                .arg(m_names[0].data()).arg(size - 1).arg(queries.size()));
            return QVariant();
        }

        // populate parameters from query and ensure each parameter is listed
        QMap<QString,QString>::iterator it;
        for (int i = 0; i < size; ++i)
        {
            parameters[i] = QMetaType::create(m_types[i]);
            if (i)
            {
                it = queries.find(m_names[i]);
                if (it == queries.end())
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("Parameter '%1' for method '%2' is missing")
                        .arg(m_names[i].data()).arg(m_names[0].data()));
                    return QVariant();
                }
                SetValue(parameters[i], it.value(), m_types[i]);
            }
        }

        Object->qt_metacall(QMetaObject::InvokeMetaMethod, m_index, parameters);

        QVariant result = m_types[0] == QMetaType::Void ? QVariant() : QVariant(m_types[0], parameters[0]);

        // free allocated parameters
        for (int i = 0; i < size; ++i)
            if (parameters[i])
                QMetaType::destroy(m_types.data()[i], parameters[i]);

        ReturnType = m_returnType;
        return result;
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

    bool                m_valid;
    int                 m_index;
    QVector<QByteArray> m_names;
    QVector<int>        m_types;
    int                 m_allowedRequestTypes;
    QString             m_returnType;
};

TorcHTTPService::TorcHTTPService(QObject *Parent, const QString &Signature, const QString &Name,
                                 const QMetaObject &MetaObject, const QString &Blacklist)
  : TorcHTTPHandler(SERVICES_DIRECTORY + Signature, Name),
    m_parent(Parent),
    m_metaObject(MetaObject)
{
    QStringList blacklist = Blacklist.split(",");

    // analyse available methods
    for (int i = 0; i < m_metaObject.methodCount(); ++i)
    {
        QMetaMethod method = m_metaObject.method(i);

        if ((method.methodType() == QMetaMethod::Slot) &&
            (method.access()     == QMetaMethod::Public))
        {
            QString name(method.methodSignature());
            name = name.section('(', 0, 0);

            // discard unwanted slots
            if (name == "deleteLater" || blacklist.contains(name))
                continue;

            // any Q_CLASSINFO for this method?
            // current 'schema' allows specification of allowed HTTP methods (PUT, GET etc)
            // and custom return types, which are used to improve the usability of maps and
            // lists when returned via XML, JSON, PLIST etc
            QString returntype;
            int customallowed = HTTPUnknownType;

            int index = m_metaObject.indexOfClassInfo(name.toLatin1());
            if (index > -1)
            {
                QStringList infos = QString(m_metaObject.classInfo(index).value()).split(",", QString::SkipEmptyParts);
                foreach (QString info, infos)
                {
                    if (info.startsWith("methods="))
                        customallowed = TorcHTTPRequest::StringToAllowed(info.mid(8));
                    else if (info.startsWith("type="))
                        returntype = info.mid(5);
                }
            }

            // determine allowed request types
            int allowed = HTTPOptions;
            if (customallowed != HTTPUnknownType)
            {
                allowed |= customallowed;
            }
            else if (name.startsWith("Get", Qt::CaseInsensitive))
            {
                allowed += HTTPGet | HTTPHead;
            }
            else if (name.startsWith("Set", Qt::CaseInsensitive))
            {
                // TODO Put or Post?? How to handle head requests for setters...
                allowed += HTTPPut;
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Unable to determine request types of method '%1' - ignoring").arg(name));
                continue;
            }

            MethodParameters *parameters = new MethodParameters(i, method, allowed, returntype);

            if (parameters->m_valid)
                m_methods.insert(name, parameters);
            else
                delete parameters;
        }
    }

    TorcHTTPServer::RegisterHandler(this);
}

TorcHTTPService::~TorcHTTPService()
{
    TorcHTTPServer::DeregisterHandler(this);

    qDeleteAll(m_methods);
}

void TorcHTTPService::ProcessHTTPRequest(TorcHTTPServer *Server, TorcHTTPRequest *Request, TorcHTTPConnection *Connection)
{
    QString method = Request->GetMethod();
    HTTPRequestType type = Request->GetHTTPRequestType();

    if (method.compare("help", Qt::CaseInsensitive) == 0)
    {
        if (type == HTTPOptions)
        {
            Request->SetStatus(HTTP_OK);
            Request->SetResponseType(HTTPResponseDefault);
            Request->SetAllowed(HTTPHead | HTTPGet | HTTPOptions);
            return;
        }

        if (type != HTTPGet && type != HTTPHead)
        {
            Request->SetStatus(HTTP_BadRequest);
            Request->SetResponseType(HTTPResponseDefault);
            return;
        }

        UserHelp(Server, Request, Connection);
        return;
    }

    QMap<QString,MethodParameters*>::iterator it = m_methods.find(method);
    if (it != m_methods.end())
    {
        // filter out invalid request types
        if (!(type & (*it)->m_allowedRequestTypes))
        {
            Request->SetStatus(HTTP_BadRequest);
            Request->SetResponseType(HTTPResponseDefault);
            return;
        }

        // handle OPTIONS
        if (type == HTTPOptions)
        {
            Request->SetStatus(HTTP_OK);
            Request->SetResponseType(HTTPResponseDefault);
            Request->SetAllowed((*it)->m_allowedRequestTypes);
            return;
        }

        QString type;
        bool    voidresult;
        QVariant result = (*it)->Invoke(m_parent, Request, type, voidresult);

        // is there a result
        if (!voidresult)
        {
            // check for invocation errors
            if (result.type() == QVariant::Invalid)
            {
                Request->SetStatus(HTTP_BadRequest);
                Request->SetResponseType(HTTPResponseDefault);
                return;
            }

            TorcSerialiser *serialiser = Request->GetSerialiser();
            Request->SetResponseType(serialiser->ResponseType());
            Request->SetResponseContent(serialiser->Serialise(result, type));
            delete serialiser;
        }
        else
        {
            Request->SetResponseType(HTTPResponseNone);
        }

        Request->SetStatus(HTTP_OK);
    }
}

void TorcHTTPService::UserHelp(TorcHTTPServer *Server, TorcHTTPRequest *Request, TorcHTTPConnection *Connection)
{
    if (!Request || !Server)
        return;

    QByteArray *result = new QByteArray();
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

            stream << method << ") (" << TorcHTTPRequest::AllowedToString(params->m_allowedRequestTypes) << ")<br>";
        }

        QString url = Connection->GetSocket() ? QString("http://") + Connection->GetSocket()->localAddress().toString()
                                                + ":" + QString::number(Connection->GetSocket()->localPort()) : QObject::tr("Error");
        QString usage = url + m_signature + example.key();

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
