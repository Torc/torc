#ifndef TORCTRANSLATION_H
#define TORCTRANSLATION_H

// Qt
#include <QVariant>
#include <QString>

// Torc
#include "torccoreexport.h"

class TORC_CORE_PUBLIC TorcStringFactory
{
  public:
    TorcStringFactory();
    virtual ~TorcStringFactory();

    static QVariantMap           GetTorcStrings         (void);
    static TorcStringFactory*    GetTorcStringFactory   (void);
    TorcStringFactory*           NextFactory            (void) const;

    virtual void                 GetStrings             (QVariantMap &Strings) = 0;

  protected:
    static TorcStringFactory*    gTorcStringFactory;
    TorcStringFactory*           nextTorcStringFactory;
};
#endif // TORCTRANSLATION_H
