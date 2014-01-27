#ifndef TORCLANGUAGE_H
#define TORCLANGUAGE_H

// Qt
#include <QMap>
#include <QLocale>
#include <QTranslator>

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
    ~TorcLanguage();
    void               LoadTranslator  (void);
    void               LoadPreferences (void);
    QLocale::Language  GetLanguage     (void);

  private:
    QLocale            m_locale;
    QTranslator       *m_translator;
};

#endif // TORCLANGUAGE_H
