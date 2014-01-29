#ifndef TORCLANGUAGE_H
#define TORCLANGUAGE_H

// Qt
#include <QMap>
#include <QLocale>
#include <QTranslator>
#include <QReadWriteLock>
#include <QAbstractListModel>

// Torc
#include "torchttpservice.h"
#include "torccoreexport.h"

#define DEFAULT_QT_LANGUAGE (QLocale::AnyLanguage)

class TORC_CORE_PUBLIC TorcLanguage : public QAbstractListModel, public TorcHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",   "1.0.0")
    Q_PROPERTY(QString languageCode   READ GetLanguageCode   WRITE SetLanguageCode NOTIFY LanguageCodeChanged)
    Q_PROPERTY(QString languageString READ GetLanguageString NOTIFY LanguageStringChanged)
    Q_PROPERTY(QVariantMap languages  READ GetLanguages      CONSTANT)

  public:
    static QMap<QString,int> gLanguageMap;
    static QString           ToString              (QLocale::Language Language, bool Empty = false);
    static QLocale::Language From2CharCode         (const char *Code);
    static QLocale::Language From2CharCode         (const QString &Code);
    static QLocale::Language From3CharCode         (const char *Code);
    static QLocale::Language From3CharCode         (const QString &Code);

  public:
    TorcLanguage();
    virtual ~TorcLanguage();

    void                     LoadPreferences       (void);
    QLocale                  GetLocale             (void);

  signals:
    void                     LanguageCodeChanged   (QString Language);
    void                     LanguageStringChanged (QString String);

  public slots:
    void                     SetLanguageCode       (const QString &Language);
    QString                  GetLanguageCode       (void);
    QString                  GetLanguageString     (void);
    QVariantMap              GetLanguages          (void);
    QString                  GetTranslation        (const QString &Context, const QString &String,
                                                    const QString &Disambiguation, int Number);
    void                     SubscriberDeleted     (QObject *Subscriber);

  public:
    // QAbstractListModel
    QVariant                 data                  (const QModelIndex &Index, int Role) const;
    QHash<int,QByteArray>    roleNames             (void) const;
    int                      rowCount              (const QModelIndex &Parent = QModelIndex()) const;

  private:
    void                     InitialiseTranslations (void);
    static void              Initialise            (void);

  private:
    QString                  languageCode;
    QString                  languageString;
    QLocale                  m_locale;
    QList<QLocale>           m_languages;
    QVariantMap              languages; // dummy
    QTranslator             *m_translator;
    QReadWriteLock          *m_lock;
};

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

#endif // TORCLANGUAGE_H
