#ifndef TORCSQLITEDB_H
#define TORCSQLITEDB_H

#include "torcdb.h"

class TorcSQLiteDB : public TorcDB
{
  public:
    TorcSQLiteDB(const QString &DatabaseName);

  protected:
    bool InitDatabase(void);
};

#endif // TORCSQLITEDB_H
