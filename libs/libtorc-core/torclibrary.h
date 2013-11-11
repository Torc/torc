#ifndef TORCLIBRARY_H
#define TORCLIBRARY_H

// Qt
#include <QLibrary>

// Torc
#include "torccoreexport.h"

class TORC_CORE_PUBLIC TorcLibrary : public QLibrary
{
  public:
    TorcLibrary(const QString &FileName, int Version);
    TorcLibrary(const QString &FileName);
    virtual ~TorcLibrary();

  private:
    void Load (void);
};

#endif // TORCLIBRARY_H
