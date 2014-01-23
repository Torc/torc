#ifndef TORCTRANSLATION_H
#define TORCTRANSLATION_H

// Qt
#include <QMap>
#include <QString>

// Torc
#include "torccoreexport.h"

class TORC_CORE_PUBLIC TorcStringFactory
{
  public:
    TorcStringFactory();
    virtual ~TorcStringFactory();

    static QMap<QString,QString> GetTorcStrings         (void);
    static TorcStringFactory*    GetTorcStringFactory   (void);
    TorcStringFactory*           NextFactory            (void) const;

    virtual void                 GetStrings             (QMap<QString,QString> &Strings) = 0;

  protected:
    static TorcStringFactory*    gTorcStringFactory;
    TorcStringFactory*           nextTorcStringFactory;
};
#endif // TORCTRANSLATION_H
