/* Class TorcLanguage
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
#include <QDir>
#include <QCoreApplication>

// Torc
#include "torclogging.h"
#include "torclocalcontext.h"
#include "torcdirectories.h"
#include "torclanguage.h"

QMap<QString,int> TorcLanguage::gLanguageMap;

#define BLACKLIST QString("submit,revert")

/*! \class TorcLanguage
 *  \brief A class to track and manage language and locale settings and available translations.
 *
 * TorcLanguage uses the 2 or 4 character language/country code to identify a language (e.g. en_GB, en_US, en).
 * If a specific language has not been set in the settings database, the system language will be used (with a
 * final fallback to en_GB). The current language can be set with SetLanguageCode and retrieved with GetLanguageCode.
 * GetLanguageString returns a translated, user presentable string naming the language.
 *
 * GetLanguages returns a list of available translations.
 *
 * GetTranslation provides a context sensitive translation service for strings. It is intended for dynamically
 * translated strings (e.g. plurals) retrieved via the HTTP interface. Other strings should be loaded once only.
 *
 * Various utility functions are available for interfacing with 3rd party libraries (From2CharCode etc).
 *
 * TorcLanguage is available from QML via QAbstractListModel inheritance and from the HTTP interface via TorcHTTPService.
 *
 * \todo Check whether QTranslator::load is thread safe.
 * \todo Add support for multiple translation files (e.g. plugins as well ).
*/
TorcLanguage::TorcLanguage()
  : QAbstractListModel(),
    TorcHTTPService(this, "languages", "languages", TorcLanguage::staticMetaObject, BLACKLIST),
    m_translator(new QTranslator()),
    m_lock(new QReadWriteLock(QReadWriteLock::Recursive))
{
    QCoreApplication::installTranslator(m_translator);

    LOG(VB_GENERAL, LOG_INFO, QString("System language: %1 (%2) (%3)(env - %4)")
        .arg(QLocale::languageToString(m_locale.language()))
        .arg(QLocale::countryToString(m_locale.country()))
        .arg(m_locale.name()).arg(qgetenv("LANG").data()));

    Initialise();
}

TorcLanguage::~TorcLanguage()
{
    {
        QWriteLocker locker(m_lock);
        QCoreApplication::removeTranslator(m_translator);
        delete m_translator;
    }

    delete m_lock;
}

QVariant TorcLanguage::data(const QModelIndex &Index, int Role) const
{
    QReadLocker locker(m_lock);

    int row = Index.row();

    if (row < 0 || row >= m_languages.size())
        return QVariant();

    switch (Role)
    {
        case (Qt::UserRole + 2):
            return QVariant::fromValue(m_languages.at(row).name());
        case (Qt::UserRole + 1):
        default:
            break;
    }

    return QVariant::fromValue(m_languages.at(row).nativeLanguageName());
}

QHash<int,QByteArray> TorcLanguage::roleNames(void) const
{
    QReadLocker locker(m_lock);
    QHash<int,QByteArray> roles;
    roles.insert(Qt::UserRole + 1, "languageCode");
    roles.insert(Qt::UserRole + 2, "languageString");
    return roles;
}

int TorcLanguage::rowCount(const QModelIndex&) const
{
    QReadLocker locker(m_lock);
    return m_languages.size();
}

/*! \brief Set the current language for this application.
 *
 * \note We let QTranslator provide the heuristics around fallback translations (e.g. en_GB to en) and hence
 *       do not validate the new language/locale against known/available translations.
*/
void TorcLanguage::SetLanguageCode(const QString &Language)
{
    QWriteLocker locker(m_lock);

    // ignore unnecessary changes
    QLocale locale(Language);
    if (m_locale == locale)
    {
        LOG(VB_GENERAL, LOG_INFO, "Requested language already set - ignoring");
        return;
    }

    // set new details
    m_locale       = locale;
    languageCode   = m_locale.name();
    languageString = m_locale.nativeLanguageName();

    LOG(VB_GENERAL, LOG_INFO, QString("Language changed: %1 (%2) (%3)(env - %4)")
        .arg(QLocale::languageToString(m_locale.language()))
        .arg(QLocale::countryToString(m_locale.country()))
        .arg(m_locale.name()).arg(qgetenv("LANG").data()));

    // load the new translation. This will replace the existing translation.
    // NB it's not clear from the docs whether this is thread safe.
    // NB this only supports installing a single translation file. So other 'modules' cannot
    // currently be loaded.

    QString filename = QString("torc_%1.qm").arg(m_locale.name());
    if (!m_translator->load(filename, GetTorcTransDir()))
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to load translation file '%1' from '%2'").arg(filename).arg(GetTorcTransDir()));

    // notify change
    emit LanguageCodeChanged(languageCode);
    emit LanguageStringChanged(languageString);
}

/// \brief Return the current language.
QString TorcLanguage::GetLanguageCode(void)
{
    QReadLocker locker(m_lock);
    return languageCode;
}

QLocale TorcLanguage::GetLocale(void)
{
    QReadLocker locker(m_lock);
    return m_locale;
}

QString TorcLanguage::GetLanguageString(void)
{
    QReadLocker locker(m_lock);
    return languageString;
}

QVariantMap TorcLanguage::GetLanguages(void)
{
    QReadLocker locker(m_lock);

    QVariantMap results;
    for (int i = 0; i < m_languages.size(); ++i)
        results.insert(m_languages.at(i).name(), m_languages.at(i).nativeLanguageName());
    return results;
}

QString TorcLanguage::GetTranslation(const QString &Context, const QString &String, const QString &Disambiguation, int Number)
{
    return QCoreApplication::translate(String.toUtf8().constData(), Context.toUtf8().constData(),
                                       Disambiguation.toUtf8().constData(), Number);
}

void TorcLanguage::SubscriberDeleted(QObject *Subscriber)
{
    TorcHTTPService::HandleSubscriberDeleted(Subscriber);
}

/*! \brief Load the user's preferred Language and Country settings
 *
 * If a language has not been specifically set and stored in the database, then we fall back
 * to the system language, otherwise assume en_GB.
*/
void TorcLanguage::LoadPreferences(void)
{
    InitialiseTranslations();

    QString language = gLocalContext->GetSetting(TORC_CORE + "Language", QString(""));

    if (language.isEmpty())
    {
        language = m_locale.name(); // somewhat circular

        if (language.isEmpty())
            language = "en_GB";
    }

    SetLanguageCode(language);
}

/// \brief Return a user readable string for the current language.
QString TorcLanguage::ToString(QLocale::Language Language, bool Empty)
{
    if (Language == DEFAULT_QT_LANGUAGE)
        return Empty ? QString("") : QString("Unknown");

    return QLocale::languageToString(Language);
}

/// \brief Return the language associated with the given 2 character code.
QLocale::Language TorcLanguage::From2CharCode(const char *Code)
{
    return From2CharCode(QString(Code));
}

/// \brief Return the language associated with the given 2 character code.
QLocale::Language TorcLanguage::From2CharCode(const QString &Code)
{
    QString language = Code.toLower();

    if (language.size() == 2)
    {
        QLocale locale(Code);
        return locale.language();
    }

    return DEFAULT_QT_LANGUAGE;
}

/// \brief Return the language associated with the given 3 character code.
QLocale::Language TorcLanguage::From3CharCode(const char *Code)
{
    return From3CharCode(QString(Code));
}

/// \brief Return the language associated with the given 3 character code.
QLocale::Language TorcLanguage::From3CharCode(const QString &Code)
{
    QString language = Code.toLower();

    if (language.size() == 3)
    {
        QMap<QString,int>::iterator it = gLanguageMap.find(language);
        if (it != gLanguageMap.end())
            return (QLocale::Language)it.value();
    }

    return DEFAULT_QT_LANGUAGE;
}

void TorcLanguage::InitialiseTranslations(void)
{
    QWriteLocker locker(m_lock);

    // clear out old (just in case)
    m_languages.clear();

    // retrieve list of installed translation files
    QDir directory(GetTorcTransDir());
    QStringList files = directory.entryList(QStringList("torc_*.qm"),
                                            QDir::Files | QDir::Readable | QDir::NoSymLinks | QDir::NoDotAndDotDot,
                                            QDir::Name);

    // create a reference list
    LOG(VB_GENERAL, LOG_INFO, QString("Found %1 translations").arg(files.size()));
    foreach (QString file, files)
    {
        file.chop(3);
        m_languages.append(QLocale(file.mid(5)));
    }
}

/*! \brief Initialise the list of supported languages.
 *
 * We aim to use the 3 character code internally for compatability with
 * other libraries and devices. To maintain performance, we create a static
 * list of languages mapping 3 character codes to Qt constants.
*/
void TorcLanguage::Initialise(void)
{
    static bool initialised = false;

    if (initialised)
        return;

    initialised = true;

    gLanguageMap.insert("aar", QLocale::Afar);
    gLanguageMap.insert("abk", QLocale::Abkhazian);
    gLanguageMap.insert("afr", QLocale::Afrikaans);
    gLanguageMap.insert("aka", QLocale::Akan);
    gLanguageMap.insert("alb", QLocale::Albanian);
    gLanguageMap.insert("amh", QLocale::Amharic);
    gLanguageMap.insert("ara", QLocale::Arabic);
    gLanguageMap.insert("arm", QLocale::Armenian);
    gLanguageMap.insert("asm", QLocale::Assamese);
    gLanguageMap.insert("aym", QLocale::Aymara);
    gLanguageMap.insert("aze", QLocale::Azerbaijani);
    gLanguageMap.insert("bak", QLocale::Bashkir);
    gLanguageMap.insert("bam", QLocale::Bambara);
    gLanguageMap.insert("baq", QLocale::Basque);
    gLanguageMap.insert("bel", QLocale::Byelorussian);
    gLanguageMap.insert("ben", QLocale::Bengali);
    gLanguageMap.insert("bih", QLocale::Bihari);
    gLanguageMap.insert("bis", QLocale::Bislama);
    gLanguageMap.insert("bos", QLocale::Bosnian);
    gLanguageMap.insert("bre", QLocale::Breton);
    gLanguageMap.insert("bul", QLocale::Bulgarian);
    gLanguageMap.insert("bur", QLocale::Burmese);
    gLanguageMap.insert("byn", QLocale::Blin);
    gLanguageMap.insert("cat", QLocale::Catalan);
    gLanguageMap.insert("cch", QLocale::Atsam);
    gLanguageMap.insert("chi", QLocale::Chinese);
    gLanguageMap.insert("cor", QLocale::Cornish);
    gLanguageMap.insert("cos", QLocale::Corsican);
    gLanguageMap.insert("cze", QLocale::Czech);
    gLanguageMap.insert("dan", QLocale::Danish);
    gLanguageMap.insert("div", QLocale::Divehi);
    gLanguageMap.insert("dut", QLocale::Dutch);
    gLanguageMap.insert("dzo", QLocale::Bhutani);
    gLanguageMap.insert("eng", QLocale::English);
    gLanguageMap.insert("epo", QLocale::Esperanto);
    gLanguageMap.insert("est", QLocale::Estonian);
    gLanguageMap.insert("ewe", QLocale::Ewe);
    gLanguageMap.insert("fao", QLocale::Faroese);
    gLanguageMap.insert("fil", QLocale::Filipino);
    gLanguageMap.insert("fin", QLocale::Finnish);
    gLanguageMap.insert("fre", QLocale::French);
    gLanguageMap.insert("fry", QLocale::Frisian);
    gLanguageMap.insert("ful", QLocale::Fulah);
    gLanguageMap.insert("fur", QLocale::Friulian);
    gLanguageMap.insert("gaa", QLocale::Ga);
    gLanguageMap.insert("geo", QLocale::Georgian);
    gLanguageMap.insert("ger", QLocale::German);
    gLanguageMap.insert("gez", QLocale::Geez);
    gLanguageMap.insert("gla", QLocale::Gaelic);
    gLanguageMap.insert("gle", QLocale::Irish);
    gLanguageMap.insert("glg", QLocale::Galician);
    gLanguageMap.insert("glv", QLocale::Manx);
    gLanguageMap.insert("gre", QLocale::Greek);
    gLanguageMap.insert("grn", QLocale::Guarani);
    gLanguageMap.insert("guj", QLocale::Gujarati);
    gLanguageMap.insert("hau", QLocale::Hausa);
    gLanguageMap.insert("haw", QLocale::Hawaiian);
    gLanguageMap.insert("hbs", QLocale::SerboCroatian);
    gLanguageMap.insert("heb", QLocale::Hebrew);
    gLanguageMap.insert("hin", QLocale::Hindi);
    gLanguageMap.insert("hun", QLocale::Hungarian);
    gLanguageMap.insert("ibo", QLocale::Igbo);
    gLanguageMap.insert("ice", QLocale::Icelandic);
    gLanguageMap.insert("iii", QLocale::SichuanYi);
    gLanguageMap.insert("iku", QLocale::Inuktitut);
    gLanguageMap.insert("ile", QLocale::Interlingue);
    gLanguageMap.insert("ina", QLocale::Interlingua);
    gLanguageMap.insert("ind", QLocale::Indonesian);
    gLanguageMap.insert("ipk", QLocale::Inupiak);
    gLanguageMap.insert("ita", QLocale::Italian);
    gLanguageMap.insert("jav", QLocale::Javanese);
    gLanguageMap.insert("jpn", QLocale::Japanese);
    gLanguageMap.insert("kaj", QLocale::Jju);
    gLanguageMap.insert("kal", QLocale::Greenlandic);
    gLanguageMap.insert("kan", QLocale::Kannada);
    gLanguageMap.insert("kam", QLocale::Kamba);
    gLanguageMap.insert("kas", QLocale::Kashmiri);
    gLanguageMap.insert("kaz", QLocale::Kazakh);
    gLanguageMap.insert("kcg", QLocale::Tyap);
    gLanguageMap.insert("kfo", QLocale::Koro);
    gLanguageMap.insert("khm", QLocale::Cambodian);
    gLanguageMap.insert("kik", QLocale::Kikuyu);
    gLanguageMap.insert("kin", QLocale::Kinyarwanda);
    gLanguageMap.insert("kir", QLocale::Kirghiz);
    gLanguageMap.insert("kok", QLocale::Konkani);
    gLanguageMap.insert("kor", QLocale::Korean);
    gLanguageMap.insert("kpe", QLocale::Kpelle);
    gLanguageMap.insert("kur", QLocale::Kurdish);
    gLanguageMap.insert("lat", QLocale::Latin);
    gLanguageMap.insert("lav", QLocale::Latvian);
    gLanguageMap.insert("lin", QLocale::Lingala);
    gLanguageMap.insert("lit", QLocale::Lithuanian);
    gLanguageMap.insert("lug", QLocale::Ganda);
    gLanguageMap.insert("mac", QLocale::Macedonian);
    gLanguageMap.insert("mal", QLocale::Malayalam);
    gLanguageMap.insert("mao", QLocale::Maori);
    gLanguageMap.insert("mar", QLocale::Marathi);
    gLanguageMap.insert("may", QLocale::Malay);
    gLanguageMap.insert("mlg", QLocale::Malagasy);
    gLanguageMap.insert("mlt", QLocale::Maltese);
    gLanguageMap.insert("mol", QLocale::Moldavian);
    gLanguageMap.insert("mon", QLocale::Mongolian);
    gLanguageMap.insert("nau", QLocale::NauruLanguage);
    gLanguageMap.insert("nbl", QLocale::SouthNdebele);
    gLanguageMap.insert("nde", QLocale::NorthNdebele);
    gLanguageMap.insert("nds", QLocale::LowGerman);
    gLanguageMap.insert("nep", QLocale::Nepali);
    gLanguageMap.insert("nno", QLocale::NorwegianNynorsk);
    gLanguageMap.insert("nob", QLocale::NorwegianBokmal);
    gLanguageMap.insert("nor", QLocale::Norwegian);
    gLanguageMap.insert("nya", QLocale::Chewa);
    gLanguageMap.insert("oci", QLocale::Occitan);
    gLanguageMap.insert("ori", QLocale::Oriya);
    gLanguageMap.insert("orm", QLocale::Afan);
    gLanguageMap.insert("pan", QLocale::Punjabi);
    gLanguageMap.insert("per", QLocale::Persian);
    gLanguageMap.insert("pol", QLocale::Polish);
    gLanguageMap.insert("por", QLocale::Portuguese);
    gLanguageMap.insert("pus", QLocale::Pashto);
    gLanguageMap.insert("que", QLocale::Quechua);
    gLanguageMap.insert("roh", QLocale::RhaetoRomance);
    gLanguageMap.insert("rum", QLocale::Romanian);
    gLanguageMap.insert("run", QLocale::Kurundi);
    gLanguageMap.insert("rus", QLocale::Russian);
    gLanguageMap.insert("san", QLocale::Sanskrit);
    gLanguageMap.insert("scc", QLocale::Serbian);
    gLanguageMap.insert("scr", QLocale::Croatian);
    gLanguageMap.insert("sid", QLocale::Sidamo);
    gLanguageMap.insert("slo", QLocale::Slovak);
    gLanguageMap.insert("slv", QLocale::Slovenian);
    gLanguageMap.insert("syr", QLocale::Syriac);
    gLanguageMap.insert("sme", QLocale::NorthernSami);
    gLanguageMap.insert("smo", QLocale::Samoan);
    gLanguageMap.insert("sna", QLocale::Shona);
    gLanguageMap.insert("snd", QLocale::Sindhi);
    gLanguageMap.insert("som", QLocale::Somali);
    gLanguageMap.insert("spa", QLocale::Spanish);
    gLanguageMap.insert("sun", QLocale::Sundanese);
    gLanguageMap.insert("swa", QLocale::Swahili);
    gLanguageMap.insert("swe", QLocale::Swedish);
    gLanguageMap.insert("tam", QLocale::Tamil);
    gLanguageMap.insert("tat", QLocale::Tatar);
    gLanguageMap.insert("tel", QLocale::Telugu);
    gLanguageMap.insert("tgk", QLocale::Tajik);
    gLanguageMap.insert("tgl", QLocale::Tagalog);
    gLanguageMap.insert("tha", QLocale::Thai);
    gLanguageMap.insert("tib", QLocale::Tibetan);
    gLanguageMap.insert("tig", QLocale::Tigre);
    gLanguageMap.insert("tir", QLocale::Tigrinya);
    gLanguageMap.insert("tso", QLocale::Tsonga);
    gLanguageMap.insert("tuk", QLocale::Turkmen);
    gLanguageMap.insert("tur", QLocale::Turkish);
    gLanguageMap.insert("twi", QLocale::Twi);
    gLanguageMap.insert("uig", QLocale::Uigur);
    gLanguageMap.insert("ukr", QLocale::Ukrainian);
    gLanguageMap.insert("urd", QLocale::Urdu);
    gLanguageMap.insert("uzb", QLocale::Uzbek);
    gLanguageMap.insert("ven", QLocale::Venda);
    gLanguageMap.insert("vie", QLocale::Vietnamese);
    gLanguageMap.insert("vol", QLocale::Volapuk);
    gLanguageMap.insert("wal", QLocale::Walamo);
    gLanguageMap.insert("wel", QLocale::Welsh);
    gLanguageMap.insert("wol", QLocale::Wolof);
    gLanguageMap.insert("xho", QLocale::Xhosa);
    gLanguageMap.insert("yid", QLocale::Yiddish);
    gLanguageMap.insert("yor", QLocale::Yoruba);
    gLanguageMap.insert("zha", QLocale::Zhuang);
    gLanguageMap.insert("zul", QLocale::Zulu);

    gLanguageMap.insert("sqi", QLocale::Albanian);
    gLanguageMap.insert("hye", QLocale::Armenian);
    gLanguageMap.insert("eus", QLocale::Basque);
    gLanguageMap.insert("mya", QLocale::Burmese);
    gLanguageMap.insert("zho", QLocale::Chinese);
    gLanguageMap.insert("ces", QLocale::Czech);
    gLanguageMap.insert("nld", QLocale::Dutch);
    gLanguageMap.insert("fra", QLocale::French);
    gLanguageMap.insert("kat", QLocale::Georgian);
    gLanguageMap.insert("deu", QLocale::German);
    gLanguageMap.insert("ell", QLocale::Greek);
    gLanguageMap.insert("isl", QLocale::Icelandic);
    gLanguageMap.insert("mkd", QLocale::Macedonian);
    gLanguageMap.insert("mri", QLocale::Maori);
    gLanguageMap.insert("msa", QLocale::Malay);
    gLanguageMap.insert("fas", QLocale::Persian);
    gLanguageMap.insert("ron", QLocale::Romanian);
    gLanguageMap.insert("srp", QLocale::Serbian);
    gLanguageMap.insert("hrv", QLocale::Croatian);
    gLanguageMap.insert("slk", QLocale::Slovak);
    gLanguageMap.insert("bod", QLocale::Tibetan);
    gLanguageMap.insert("cym", QLocale::Welsh);
    gLanguageMap.insert("chs", QLocale::Chinese);

    gLanguageMap.insert("nso", QLocale::NorthernSotho);
    gLanguageMap.insert("trv", QLocale::Taroko);
    gLanguageMap.insert("guz", QLocale::Gusii);
    gLanguageMap.insert("dav", QLocale::Taita);
    gLanguageMap.insert("saq", QLocale::Samburu);
    gLanguageMap.insert("seh", QLocale::Sena);
    gLanguageMap.insert("rof", QLocale::Rombo);
    gLanguageMap.insert("shi", QLocale::Tachelhit);
    gLanguageMap.insert("kab", QLocale::Kabyle);
    gLanguageMap.insert("nyn", QLocale::Nyankole);
    gLanguageMap.insert("bez", QLocale::Bena);
    gLanguageMap.insert("vun", QLocale::Vunjo);
    gLanguageMap.insert("ebu", QLocale::Embu);
    gLanguageMap.insert("chr", QLocale::Cherokee);
    gLanguageMap.insert("mfe", QLocale::Morisyen);
    gLanguageMap.insert("kde", QLocale::Makonde);
    gLanguageMap.insert("lag", QLocale::Langi);
    gLanguageMap.insert("bem", QLocale::Bemba);
    gLanguageMap.insert("kea", QLocale::Kabuverdianu);
    gLanguageMap.insert("mer", QLocale::Meru);
    gLanguageMap.insert("kln", QLocale::Kalenjin);
    gLanguageMap.insert("naq", QLocale::Nama);
    gLanguageMap.insert("jmc", QLocale::Machame);
    gLanguageMap.insert("ksh", QLocale::Colognian);
    gLanguageMap.insert("mas", QLocale::Masai);
    gLanguageMap.insert("xog", QLocale::Soga);
    gLanguageMap.insert("luy", QLocale::Luyia);
    gLanguageMap.insert("asa", QLocale::Asu);
    gLanguageMap.insert("teo", QLocale::Teso);
    gLanguageMap.insert("ssy", QLocale::Saho);
    gLanguageMap.insert("khq", QLocale::KoyraChiini);
    gLanguageMap.insert("rwk", QLocale::Rwa);
    gLanguageMap.insert("luo", QLocale::Luo);
    gLanguageMap.insert("cgg", QLocale::Chiga);
    gLanguageMap.insert("tzm", QLocale::CentralMoroccoTamazight);
    gLanguageMap.insert("ses", QLocale::KoyraboroSenni);
    gLanguageMap.insert("ksb", QLocale::Shambala);
    gLanguageMap.insert("gsw", QLocale::SwissGerman);
    gLanguageMap.insert("fij", QLocale::Fijian);
    gLanguageMap.insert("lao", QLocale::Lao);
    gLanguageMap.insert("sag", QLocale::Sango);
    gLanguageMap.insert("sin", QLocale::Sinhala);
    gLanguageMap.insert("sot", QLocale::SouthernSotho);
    gLanguageMap.insert("ssw", QLocale::Swati);
    gLanguageMap.insert("ton", QLocale::Tongan);
    gLanguageMap.insert("tsn", QLocale::Tswana);
}

/*! \class TorcStringFactory
 *  \brief A factory class to register translatable strings for use with external interfaces/applications.
 *
 * A translatable string is registered with a string constant that should uniquely identify it. The list
 * of registered constants and their *current* translations can be retrieved with GetTorcStrings.
 *
 * Objects that wish to register strings should sub-class TorcStringFactory and implement GetStrings.
 *
 * The string list is made available to web interfaces via the dynamically generated torcconfiguration.js file
 * and is exported directly to all QML contexts.
*/
TorcStringFactory* TorcStringFactory::gTorcStringFactory = NULL;

TorcStringFactory::TorcStringFactory()
{
    nextTorcStringFactory = gTorcStringFactory;
    gTorcStringFactory = this;
}

TorcStringFactory::~TorcStringFactory()
{
}

TorcStringFactory* TorcStringFactory::GetTorcStringFactory(void)
{
    return gTorcStringFactory;
}

TorcStringFactory* TorcStringFactory::NextFactory(void) const
{
    return nextTorcStringFactory;
}

/*! \brief Return a map of string constants and their translations.
*/
QVariantMap TorcStringFactory::GetTorcStrings(void)
{
    QVariantMap strings;
    TorcStringFactory* factory = TorcStringFactory::GetTorcStringFactory();
    for ( ; factory; factory = factory->NextFactory())
        factory->GetStrings(strings);
    return strings;
}
