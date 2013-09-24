/* Class TorcSQLiteDB
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

// Torc
#include "torclogging.h"
#include "torcsqlitedb.h"

/*! \class TorcSQLiteDB
 *  \brief SQLite implementation of TorcDB
 *
 * All Sql specific code resides in TorcDB. TorcSQLiteDB applies SQLite specifics
 * when creating the database and upon first use after startup.
 *
 * \todo Check the exclusive lock handling. It's unclear whether this will lock out other threads.
 *
 * \sa TorcDB
*/
TorcSQLiteDB::TorcSQLiteDB(const QString &DatabaseName)
  : TorcDB(DatabaseName, "QSQLITE")
{
    InitDatabase();
}

/*! \fn    TorcSQLiteDB::InitDatabase
 *  \brief Create and/or open an SQLiteDatabase
*/
bool TorcSQLiteDB::InitDatabase(void)
{
    LOG(VB_GENERAL, LOG_INFO, QString("Attempting to open '%1'")
        .arg(m_databaseName));

    QSqlDatabase db = QSqlDatabase::database(GetThreadConnection());
    DebugError(&db);
    if (!db.isValid())
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to get valid database connection.");
        return false;
    }

    if (!db.open())
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to open database.");
        return false;
    }

    QSqlQuery query(db);

    // ensure this is the only application accessing this database
    query.exec("PRAGMA locking_mode = EXCLUSIVE");
    if (DebugError(&query))
        return false;

    query.exec("BEGIN EXCLUSIVE");
    if (DebugError(&query))
        return false;

    db.commit();
    if (DebugError(&db))
        return false;

    // Create the settings table if it doesn't exist
    query.exec("CREATE TABLE IF NOT EXISTS settings "
               "( name VARCHAR(128) NOT NULL,"
               "  value VARCHAR(16000) NOT NULL );");
    DebugError(&query);

    // Create the preferences table if it doesn't exist
    query.exec("CREATE TABLE IF NOT EXISTS preferences "
               "( name VARCHAR(128) NOT NULL,"
               "  value VARCHAR(16000) NOT NULL );");
    DebugError(&query);

    // Check the creation date for existing installations
    query.exec("SELECT value FROM settings where name='DB_DateCreated'");
    DebugError(&query);

    query.first();
    if (query.isValid())
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Settings table was created on %1")
            .arg(query.value(0).toString()));
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Initialising settings table."));
        QString createdon = QDateTime::currentDateTime().toUTC().toString(Qt::ISODate);
        query.exec(
            QString("INSERT INTO settings (name, value) VALUES ('DB_DateCreated', '%1');")
                .arg(createdon));
        DebugError(&query);
    }

    // optimise the database
    query.exec("PRAGMA page_size = 4096");
    DebugError(&query);
    query.exec("PRAGMA cache_size = 16384");
    DebugError(&query);
    query.exec("PRAGMA temp_store = MEMORY");
    DebugError(&query);
    query.exec("PRAGMA journal_mode = OFF");
    DebugError(&query);
    query.exec("PRAGMA synchronous = OFF");
    DebugError(&query);

    m_databaseValid = true;

    return true;
}
