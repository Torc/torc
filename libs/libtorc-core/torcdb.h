#ifndef TORCDB_H
#define TORCDB_H

class QSqlDatabase;
class QSqlQuery;
class TorcDBPriv;

class TorcDB
{
  public:
    TorcDB(const QString &DatabaseName, const QString &DatabaseType);
   ~TorcDB();

    static void  DebugError(QSqlQuery *Query);
    static void  DebugError(QSqlDatabase *Database);
    bool         IsValid(void);
    void         CloseThreadConnection(void);

    void         LoadSettings(QMap<QString,QString> &Settings);
    void         SetSetting(const QString &Name, const QString &Value);

  protected:
    virtual bool InitDatabase(void) = 0;
    QString      GetThreadConnection(void);

  protected:
    bool         m_databaseValid;
    QString      m_databaseName;
    QString      m_databaseType;
    TorcDBPriv  *m_databasePriv;
};

#endif // TORCDB_H
