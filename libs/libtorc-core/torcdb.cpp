/* Class TorcDB
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
#include <QtSql>
#include <QThread>
#include <QHash>

// Torc
#include "torclogging.h"
#include "torcdb.h"

/*! \class TorcDBPriv
 *  \brief Private implementation of Sql database management.
 *
 * TorcDBPriv handles multi-threaded access to the database engine.
 *
 * By default, QSql database access is not thread safe and a new connection must be opened for
 * each thread that wishes to use the database. TorcDB (and its subclasses) will ask
 * for a connection each time the database is accessed and connections are cached on a per-thread
 * basis. The thread name (and pointer) is used for cache tracking and thus any thread must have
 * a name. This implicitly limits database access to a QThread or one its subclasses (e.g. TorcQThread) and
 * hence database access will not work from QRunnable's or other miscellaneous threads - this is by design
 * and is NOT a bug.
 *
 * \sa TorcDB
 * \sa TorcSQLiteDB
*/
class TorcDBPriv
{
  public:
    TorcDBPriv(const QString &Name, const QString &Type)
      : m_name(Name),
        m_type(Type),
        m_lock(new QMutex(QMutex::Recursive))
    {
    }

    ~TorcDBPriv()
    {
        CloseConnections();

        delete m_lock;
        m_lock = NULL;
    }

    /*! \fn    TorcDBPriv::CloseConnections
     *  \brief Close all cached database connections.
    */
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

/*! \fn    TorcDBPriv::GetThreadConnection
 *  \brief Retrieve a database connection for the current thread.
 *
 * Database connections are cached (thus limiting connections to one per thread).
 *
 * \sa CloseConnections
 * \sa CloseThreadConnection
*/
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
            LOG(VB_GENERAL, LOG_ERR, "Database access is only available from TorcQThread");
            return QString();
        }

        QString name = QString("%1-%2")
                           .arg(thread->objectName())
                           .arg(QString::number((unsigned long long)thread));
        QSqlDatabase newdb = QSqlDatabase::addDatabase(m_type, name);

        if (m_type == "QSQLITE")
            newdb.setConnectOptions("QSQLITE_BUSY_TIMEOUT=1");
        newdb.setDatabaseName(m_name);

        {
            QMutexLocker locker(m_lock);
            m_connectionMap.insert(thread, name);
            LOG(VB_GENERAL, LOG_INFO, QString("New connection '%1'").arg(name));
        }

        return name;
    }

    /*! \fn    TorcDBPriv::CloseThreadConnection
     *  \brief Close the database connection for the current thread.
    */
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

/*! \class TorcDB
 *  \brief Base Sql database access class.
 *
 * TorcDB is the base implementation for Sql database access. It is currently limited
 * to setting and retrieving preferences and settings and whilst designed for generic Sql
 * usage, the only current concrete subclass is TorcSQliteDB. Changes may (will) be required for
 * other database types and usage.
 *
 * \sa TorcDBPriv
 * \sa TorcSQLiteDB
 * \sa TorcLocalContext
*/
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

/*! \fn    TorcDB::IsValid
 *  \brief Returns true if the datbase has been opened/created.
*/
bool TorcDB::IsValid(void)
{
    return m_databaseValid;
}

/*! \fn TorcDB::CloseThreadConnection
 *
 * \sa TorcDBPriv::CloseThreadConnection
*/
void TorcDB::CloseThreadConnection(void)
{
    m_databasePriv->CloseThreadConnection();
}

/*! \fn TorcDB::GetThreadConnection
 *
 * \sa TorcDBPriv::GetThreadConnection
*/
QString TorcDB::GetThreadConnection(void)
{
    return m_databasePriv->GetThreadConnection();
}

/*! \fn    TorcDB::DebugError(QSqlQuery*)
 *  \brief Log database errors following a failed query.
 *
 * \sa Debugerror(QSqlDatabase*)
*/
bool TorcDB::DebugError(QSqlQuery *Query)
{
    if (!Query)
        return true;

    QSqlError error = Query->lastError();

    if (error.type() == QSqlError::NoError)
        return false;

    if (!error.databaseText().isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Database Error: %1")
            .arg(error.databaseText()));
        return true;
    }

    if (!error.driverText().isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Driver Error: %1")
            .arg(error.driverText()));
    }

    return true;
}

/*! \fn    TorcDB::DebugError(QSqlDatabase*)
 *  \brief Log database errors following database creation.
 *
 * \sa Debugerror(QSqlQuery*)
*/
bool TorcDB::DebugError(QSqlDatabase *Database)
{
    if (!Database)
        return true;

    QSqlError error = Database->lastError();

    if (error.type() == QSqlError::NoError)
        return false;

    if (!error.databaseText().isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Database Error: %1")
            .arg(error.databaseText()));
        return true;
    }

    if (!error.driverText().isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Driver Error: %1")
            .arg(error.driverText()));
    }

    return true;
}

/*! \fn    TorcDB::LoadSettings
 *  \brief Retrieve all persistent settings stored in the database.
 *
 * Both settings and preferences are key/value pairs (of strings).
 *
 * Settings are designed as application wide configurable settings that can
 * only changed by the administrative user (if present, otherwise the default user).
 *
 * Preferences are designed as configurable, user specific behaviour that are modified
 * by the current user.
 *
 * For example, a preference might set the level of contrast while playing video content and
 * a setting would enable or disable all external network acccess (thus preventing access to
 * 'undesirable' content).
 *
 * \sa SetSetting
 * \sa SetPreference
 * \sa LoadPreferences
*/
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

/*! \fn    TorcDB::LoadPreferences
 *  \brief Retrieve all persistent preferences stored in the database for the current user.
 *
 * Preferences are per user and hence this function MUST be called if the user is changed.
 *
 * \sa SetPreference
 * \sa LoadSettings
 * \sa SetSetting
*/
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

/*! \fn    TorcDB::SetSetting
 *  \brief Set the setting Name to the value Value.
 *
 * \sa SetPreference
*/
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

/*! \fn    TorcDB::SetPreference
 *  \brief Set the preference Name to the value Value for the current user.
 *
 * \sa SetSetting
*/
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
