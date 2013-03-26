/* Class TorcDB
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
#include <QtSql>
#include <QThread>
#include <QHash>

// Torc
#include "torclogging.h"
#include "torcdb.h"

class TorcDBPriv
{
  public:
    TorcDBPriv(const QString &Name, const QString &Type)
      : m_name(Name), m_type(Type), m_lock(new QMutex(QMutex::Recursive))
    {
    }

    ~TorcDBPriv()
    {
        CloseConnections();

        delete m_lock;
        m_lock = NULL;
    }

    void CloseConnections(void)
    {
        QMutexLocker locker(m_lock);

        CloseThreadConnection();

        if (!m_connectionMap.size())
            return;

        LOG(VB_GENERAL, LOG_WARNING, QString("%1 open connections.").arg(m_connectionMap.size()));

        QHashIterator<QThread*,QString> it(m_connectionMap);
        while (it.hasNext())
        {
            it.next();
            QString name = it.value();
            LOG(VB_GENERAL, LOG_INFO, QString("Removing connection '%1'").arg(name));
            QSqlDatabase::removeDatabase(name);
        }
        m_connectionMap.clear();
    }

    QString GetThreadConnection(void)
    {
        QThread* thread = QThread::currentThread();

        {
            QMutexLocker locker(m_lock);
            if (m_connectionMap.contains(thread))
                return m_connectionMap.value(thread);
        }

        if (thread->objectName().isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR, "Database access is only available from TorcThread");
            return QString();
        }

        QString name = QString("%1-%2")
                           .arg(thread->objectName())
                           .arg(QString::number((unsigned long long)thread));
        QSqlDatabase newdb = QSqlDatabase::addDatabase(m_type, name);
        newdb.setDatabaseName(m_name);

        {
            QMutexLocker locker(m_lock);
            m_connectionMap.insert(thread, name);
            LOG(VB_GENERAL, LOG_INFO, QString("New connection '%1'").arg(name));
        }

        return name;
    }

    void CloseThreadConnection(void)
    {
        QThread* thread = QThread::currentThread();
        QMutexLocker locker(m_lock);
        if (m_connectionMap.contains(thread))
        {
            QString name = m_connectionMap.value(thread);
            LOG(VB_GENERAL, LOG_INFO, QString("Removing connection '%1'").arg(name));
            QSqlDatabase::removeDatabase(name);
            m_connectionMap.remove(thread);
        }
    }

  private:
    QString  m_name;
    QString  m_type;
    QMutex  *m_lock;
    QHash<QThread*,QString> m_connectionMap;
};

TorcDB::TorcDB(const QString &DatabaseName, const QString &DatabaseType)
  : m_databaseValid(false),
    m_databaseName(DatabaseName),
    m_databaseType(DatabaseType),
    m_databasePriv(new TorcDBPriv(DatabaseName, DatabaseType))
{
}

TorcDB::~TorcDB()
{
    m_databaseValid = false;
    delete m_databasePriv;
    m_databasePriv = NULL;
}

bool TorcDB::IsValid(void)
{
    return m_databaseValid;
}

void TorcDB::CloseThreadConnection(void)
{
    m_databasePriv->CloseThreadConnection();
}

QString TorcDB::GetThreadConnection(void)
{
    return m_databasePriv->GetThreadConnection();
}

void TorcDB::DebugError(QSqlQuery *Query)
{
    if (!Query)
        return;

    QSqlError error = Query->lastError();

    if (error.type() == QSqlError::NoError)
        return;

    if (!error.databaseText().isEmpty())
    {
        LOG(VB_DATABASE, LOG_ERR, QString("Database Error: %1")
            .arg(error.databaseText()));
    }

    if (!error.driverText().isEmpty())
    {
        LOG(VB_DATABASE, LOG_ERR, QString("Driver Error: %1")
            .arg(error.driverText()));
    }
}

void TorcDB::DebugError(QSqlDatabase *Database)
{
    if (!Database)
        return;

    QSqlError error = Database->lastError();

    if (error.type() == QSqlError::NoError)
        return;

    if (!error.databaseText().isEmpty())
    {
        LOG(VB_DATABASE, LOG_ERR, QString("Database Error: %1")
            .arg(error.databaseText()));
    }

    if (!error.driverText().isEmpty())
    {
        LOG(VB_DATABASE, LOG_ERR, QString("Driver Error: %1")
            .arg(error.driverText()));
    }
}

void TorcDB::LoadSettings(QMap<QString, QString> &Settings)
{
    QSqlDatabase db = QSqlDatabase::database(GetThreadConnection());
    DebugError(&db);
    if (!db.isValid() || !db.isOpen())
        return;

    QSqlQuery query("SELECT name, value from settings;", db);
    DebugError(&query);

    while (query.next())
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("'%1' : '%2'").arg(query.value(0).toString()).arg(query.value(1).toString()));
        Settings.insert(query.value(0).toString(), query.value(1).toString());
    }
}

void TorcDB::LoadPreferences(QMap<QString, QString> &Preferences)
{
    QSqlDatabase db = QSqlDatabase::database(GetThreadConnection());
    DebugError(&db);
    if (!db.isValid() || !db.isOpen())
        return;

    QSqlQuery query("SELECT name, value from preferences;", db);
    DebugError(&query);

    while (query.next())
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("'%1' : '%2'").arg(query.value(0).toString()).arg(query.value(1).toString()));
        Preferences.insert(query.value(0).toString(), query.value(1).toString());
    }
}

void TorcDB::SetSetting(const QString &Name, const QString &Value)
{
    if (Name.isEmpty())
        return;

    QSqlDatabase db = QSqlDatabase::database(GetThreadConnection());
    DebugError(&db);
    if (!db.isValid() || !db.isOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to open database connection.");
        return;
    }

    QSqlQuery query(db);
    query.prepare("DELETE FROM settings where name=:NAME;");
    query.bindValue(":NAME", Name);
    if (query.exec())
        DebugError(&query);

    query.prepare("INSERT INTO settings (name, value) "
                  "VALUES (:NAME, :VALUE);");
    query.bindValue(":NAME", Name);
    query.bindValue(":VALUE", Value);
    if (query.exec())
        DebugError(&query);
}

void TorcDB::SetPreference(const QString &Name, const QString &Value)
{
    if (Name.isEmpty())
        return;

    QSqlDatabase db = QSqlDatabase::database(GetThreadConnection());
    DebugError(&db);
    if (!db.isValid() || !db.isOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to open database connection.");
        return;
    }

    QSqlQuery query(db);
    query.prepare("DELETE FROM preferences where name=:NAME;");
    query.bindValue(":NAME", Name);
    if (query.exec())
        DebugError(&query);

    query.prepare("INSERT INTO preferences (name, value) "
                  "VALUES (:NAME, :VALUE);");
    query.bindValue(":NAME", Name);
    query.bindValue(":VALUE", Value);
    if (query.exec())
        DebugError(&query);
}
