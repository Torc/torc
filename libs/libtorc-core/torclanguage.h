#ifndef TORCLANGUAGE_H
#define TORCLANGUAGE_H

// Qt
#include <QMap>
#include <QLocale>

// Torc
#include "torccoreexport.h"

#define DEFAULT_QT_LANGUAGE (QLocale::AnyLanguage)

class TORC_CORE_PUBLIC TorcLanguage
{
  public:
    static QMap<QString,int> gLanguageMap;

    static void        Initialise      (void);
    static QString     ToString        (QLocale::Language Language, bool Empty = false);

    static QLocale::Language From2CharCode  (const char *Code);
    static QLocale::Language From2CharCode  (const QString &Code);
    static QLocale::Language From3CharCode  (const char *Code);
    static QLocale::Language From3CharCode  (const QString &Code);

  public:
    TorcLanguage();
    void               LoadPreferences (void);
    QLocale::Language  GetLanguage     (void);

  private:
    QLocale m_locale;
};

#endif // TORCLANGUAGE_H
