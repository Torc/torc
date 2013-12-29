/* Class TorcJSONRPC
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2013
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
#include <QMetaType>

// Torc
#include "torcjsonrpc.h"

QString TorcJSONRPC::QMetaTypetoJavascriptType(int MetaType)
{
    switch (MetaType)
    {
        case QMetaType::Void:
            return QString("undefined");
        case QMetaType::Bool:
            return QString("boolean");
        case QMetaType::Short:
        case QMetaType::UShort:
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::Long:
        case QMetaType::ULong:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
        case QMetaType::Double:
        case QMetaType::Float:
            return QString("number");
        case QMetaType::Char:
        case QMetaType::UChar:
        case QMetaType::QChar:
        case QMetaType::QString:
        case QMetaType::QByteArray:
        case QMetaType::QTime:
        case QMetaType::QDate:
        case QMetaType::QDateTime:
        case QMetaType::QUuid:
        case QMetaType::QUrl:
            return QString("string");
        default:
            break;
    }

    return QString("object");
}
