/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <qglobal.h>
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
//#error This file should not ever be used under Qt5 -- this is a backport only for Qt4!
#endif

#ifndef QMIMETYPE_H
#define QMIMETYPE_H

#include <qshareddata.h>
#include <qstring.h>

QT_BEGIN_HEADER
QT_BEGIN_NAMESPACE

class QMimeTypePrivate;
class QFileinfo;
class QStringList;

class QMimeType
{
public:
    QMimeType();
    QMimeType(const QMimeType &other);
    QMimeType &operator=(const QMimeType &other);
#ifdef Q_COMPILER_RVALUE_REFS
    QMimeType &operator=(QMimeType &&other)
    {
        qSwap(d, other.d);
        return *this;
    }
#endif
    void swap(QMimeType &other)
    {
        qSwap(d, other.d);
    }
    explicit QMimeType(const QMimeTypePrivate &dd);
    ~QMimeType();

    bool operator==(const QMimeType &other) const;

    inline bool operator!=(const QMimeType &other) const
    {
        return !operator==(other);
    }

    bool isValid() const;

    bool isDefault() const;

    QString name() const;
    QString comment() const;
    QString genericIconName() const;
    QString iconName() const;
    QStringList globPatterns() const;
    QStringList parentMimeTypes() const;
    QStringList allAncestors() const;
    QStringList suffixes() const;
    QString preferredSuffix() const;

    bool inherits(const QString &mimeTypeName) const;

    QString filterString() const;

protected:
    friend class QMimeTypeParserBase;
    friend class MimeTypeMapEntry;
    friend class QMimeDatabasePrivate;
    friend class QMimeXMLProvider;
    friend class QMimeBinaryProvider;
    friend class QMimeTypePrivate;

    QExplicitlySharedDataPointer<QMimeTypePrivate> d;
};

//Q_DECLARE_SHARED(QMimeType)

QT_END_NAMESPACE
QT_END_HEADER

#endif   // QMIMETYPE_H
