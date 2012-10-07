#ifndef TORCCOMMANDLINEPARSER_H
#define TORCCOMMANDLINEPARSER_H

// Std
#include <stdint.h>

// Qt
#include <QStringList>
#include <QDateTime>
#include <QSize>
#include <QMap>
#include <QString>
#include <QVariant>

// Torc
#include "torccoreexport.h"
#include "torclogging.h"
#include "torcreferencecounted.h"

class TorcCommandLineParser;

class TORC_CORE_PUBLIC CommandLineArg : public TorcReferenceCounter
{
    friend class TorcCommandLineParser;

  public:
    CommandLineArg(QString Name,     QVariant::Type Type, QVariant Default,
                   QString HelpText, QString LongHelpText);
    CommandLineArg(QString Name,     QVariant::Type Type, QVariant Default);
    CommandLineArg(QString Name);
    virtual ~CommandLineArg();

    CommandLineArg* SetGroup           (QString Group);
    void            AddKeyword         (QString Keyword);
    QString         GetName            (void) const;
    int             GetKeywordLength   (void) const;
    QString         GetHelpString      (int off, QString group = "",
                                        bool force = false) const;
    QString         GetLongHelpString  (QString     Keyword) const;
    bool            Set                (QString     Option);
    bool            Set                (QString     Option, QByteArray Value);
    void            Set                (QVariant    Value);
    CommandLineArg* SetParent          (QString     Child);
    CommandLineArg* SetParent          (QStringList Children);
    CommandLineArg* SetParentOf        (QString     Child);
    CommandLineArg* SetParentOf        (QStringList Children);
    CommandLineArg* SetChild           (QString     Parent);
    CommandLineArg* SetChild           (QStringList Parents);
    CommandLineArg* SetChildOf         (QString     Parent);
    CommandLineArg* SetChildOf         (QStringList Parents);
    CommandLineArg* SetRequiredChild   (QString     Option);
    CommandLineArg* SetRequiredChild   (QStringList Option);
    CommandLineArg* SetRequiredChildOf (QString     Option);
    CommandLineArg* SetRequiredChildOf (QStringList Option);
    CommandLineArg* SetRequires        (QString     Required);
    CommandLineArg* SetRequires        (QStringList Required);
    CommandLineArg* SetBlocks          (QString     Option);
    CommandLineArg* SetBlocks          (QStringList Options);
    CommandLineArg* SetDeprecated      (QString     Deprecated = "");
    CommandLineArg* SetRemoved         (QString     Removed = "",
                                        QString     Version = "");
    static void     AllowOneOf         (QList<CommandLineArg*> Arguments);
    void            PrintVerbose       (void) const;

  private:
    QString         GetKeywordString   (void) const;
    void            SetParentOf        (CommandLineArg *Child,    bool Forward = true);
    void            SetChildOf         (CommandLineArg *Parent,   bool Forward = true);
    void            SetRequires        (CommandLineArg *Required, bool Forward = true);
    void            SetBlocks          (CommandLineArg *Blocks,   bool Forward = true);
    QString         GetPreferredKeyword(void) const;
    bool            TestLinks          (void) const;
    void            CleanupLinks       (void);
    static void     WrapList           (QStringList &List, int Width);

    void            PrintRemovedWarning   (QString &Keyword) const;
    void            PrintDeprecatedWarning(QString &Keyword) const;


  private:
    bool            m_given;
    QString         m_name;
    QString         m_group;
    QString         m_deprecated;
    QString         m_removed;
    QString         m_removedversion;
    QVariant::Type  m_type;
    QVariant        m_default;
    QVariant        m_stored;
    QStringList     m_keywords;
    QString         m_usedKeyword;
    QString         m_helpText;
    QString         m_longHelpText;
    QList<CommandLineArg*>  m_parents;
    QList<CommandLineArg*>  m_children;
    QList<CommandLineArg*>  m_requires;
    QList<CommandLineArg*>  m_requiredby;
    QList<CommandLineArg*>  m_blocks;
};

class TORC_CORE_PUBLIC TorcCommandLineParser
{
  public:
    TorcCommandLineParser();
   ~TorcCommandLineParser();

    virtual void    LoadArguments         (void) { }
    void            PrintVersion          (void) const;
    void            PrintHelp             (void) const;
    QString         GetHelpString         (void) const;
    virtual QString GetHelpHeader         (void) const;

    virtual bool    Parse(int argc, const char * const * argv, bool &JustExit);
    CommandLineArg* Add(QString Arg, QString Name, bool        Default, QString HelpText, QString LongHelpText);
    CommandLineArg* Add(QString Arg, QString Name, int         Default, QString HelpText, QString LongHelpText);
    CommandLineArg* Add(QString Arg, QString Name, uint        Default, QString HelpText, QString LongHelpText);
    CommandLineArg* Add(QString Arg, QString Name, long long   Default, QString HelpText, QString LongHelpText);
    CommandLineArg* Add(QString Arg, QString Name, double      Default, QString HelpText, QString LongHelpText);
    CommandLineArg* Add(QString Arg, QString Name, const char *Default, QString HelpText, QString LongHelpText);
    CommandLineArg* Add(QString Arg, QString Name, QString     Default, QString HelpText, QString LongHelpText);
    CommandLineArg* Add(QString Arg, QString Name, QSize       Default, QString HelpText, QString LongHelpText);
    CommandLineArg* Add(QString Arg, QString Name, QDateTime   Default, QString HelpText, QString LongHelpText);
    CommandLineArg* Add(QString Arg, QString Name, QVariant::Type Type, QString HelpText, QString LongHelpText);
    CommandLineArg* Add(QString Arg, QString Name, QVariant::Type Type, QVariant Default, QString HelpText, QString LongHelpText);

    CommandLineArg* Add(QStringList Args, QString Name, bool        Default, QString HelpText, QString LongHelpText);
    CommandLineArg* Add(QStringList Args, QString Name, int         Default, QString HelpText, QString LongHelpText);
    CommandLineArg* Add(QStringList Args, QString Name, uint        Default, QString HelpText, QString LongHelpText);
    CommandLineArg* Add(QStringList Args, QString Name, long long   Default, QString HelpText, QString LongHelpText);
    CommandLineArg* Add(QStringList Args, QString Name, double      Default, QString HelpText, QString LongHelpText);
    CommandLineArg* Add(QStringList Args, QString Name, const char *Default, QString HelpText, QString LongHelpText);
    CommandLineArg* Add(QStringList Args, QString Name, QString     Default, QString HelpText, QString LongHelpText);
    CommandLineArg* Add(QStringList Args, QString Name, QSize       Default, QString HelpText, QString LongHelpText);
    CommandLineArg* Add(QStringList Args, QString Name, QDateTime   Default, QString HelpText, QString LongHelpText);
    CommandLineArg* Add(QStringList Args, QString Name, QVariant::Type Type, QString HelpText, QString LongHelpText);
    CommandLineArg* Add(QStringList Args, QString Name, QVariant::Type Type, QVariant Default, QString HelpText, QString LongHelpText);

    QVariant                operator[]          (const QString &Name);
    QStringList             GetArgs             (void) const;
    QMap<QString,QString>   GetExtra            (void) const;
    QString                 GetPassthrough      (void) const;
    QMap<QString,QString>   GetSettingsOverride (void);
    LogLevel                GetLoggingLevel     (void);

    bool                    ToBool              (QString Key) const;
    int                     ToInt               (QString Key) const;
    uint                    ToUInt              (QString Key) const;
    long long               ToLongLong          (QString Key) const;
    double                  ToDouble            (QString Key) const;
    QSize                   ToSize              (QString Key) const;
    QString                 ToString            (QString Key) const;
    QStringList             ToStringList        (QString Key, QString sep = "") const;
    QMap<QString,QString>   ToMap               (QString Key) const;
    QDateTime               ToDateTime          (QString Key) const;
    bool                    SetValue            (const QString &Key, QVariant Value);
    int                     Daemonize           (void);

  protected:
    void                    AllowArgs           (bool Allow = true);
    void                    AllowExtras         (bool Allow = true);
    void                    AllowPassthrough    (bool Allow = true);

    void                    AddHelp             (void);
    void                    AddVersion          (void);
    void                    AddWindowed         (void);
    void                    AddMouse            (void);
    void                    AddDaemon           (void);
    void                    AddSettingsOverride (void);
    void                    AddRecording        (void);
    void                    AddDisplay          (void);
    void                    AddUPnP             (void);
    void                    AddLogging          (const QString &defaultVerbosity = "general",
                                                 LogLevel DefaultLogging = LOG_INFO);
    void                    AddPIDFile          (void);
    void                    AddJob              (void);
    void                    AddInFile           (void);
    void                    AddOutFile          (void);

  private:
    int                     GetOpt(int argc, const char * const * argv, int &argpos,
                                   QString &Option, QByteArray &Value);
    bool                    ReconcileLinks      (void);

    QString                       m_appname;
    QMap<QString,CommandLineArg*> m_optionedArgs;
    QMap<QString,CommandLineArg*> m_namedArgs;
    bool                          m_passthroughActive;
    bool                          m_overridesImported;
    bool                          m_verbose;
};

#endif
