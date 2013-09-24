#ifndef TORCDB_H
#define TORCDB_H

class QSqlDatabase;
class QSqlQuery;
class TorcDBPriv;

class TorcDB
{
  public:
    TorcDB(const QString &DatabaseName, const QString &DatabaseType);
    virtual ~TorcDB();

    static bool  DebugError             (QSqlQuery *Query);
    static bool  DebugError             (QSqlDatabase *Database);
    bool         IsValid                (void);
    void         CloseThreadConnection  (void);

    void         LoadSettings           (QMap<QString,QString> &Settings);
    void         LoadPreferences        (QMap<QString,QString> &Preferences);
    void         SetSetting             (const QString &Name, const QString &Value);
    void         SetPreference          (const QString &Name, const QString &Value);

  protected:
    virtual bool InitDatabase           (void) = 0;
    QString      GetThreadConnection    (void);

  protected:
    bool         m_databaseValid;
    QString      m_databaseName;
    QString      m_databaseType;
    TorcDBPriv  *m_databasePriv;
};

#endif // TORCDB_H
