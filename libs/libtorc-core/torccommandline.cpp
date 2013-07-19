/* Class TorcCommandLine
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2013
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
#include <QHash>
#include <QVariant>
#include <QStringList>
#include <QCoreApplication>

// Torc
#include "torcconfig.h"
#include "version.h"
#include "torcexitcodes.h"
#include "torclogging.h"
#include "torccommandline.h"

#include <iostream>

/*! \class TorcArgument
 *  \brief Simple wrapper around a command line argument.
 *
 * The QVariant described by m_value indicates the type accepted by the argument. An
 * invalid QVariant indicates that the argument is an option only (e.g. --help) and requires
 * no value. When a value is detected in the command line, m_value is updated accordingly for
 * later retrieval, or is set to true for options to indicate the option was detected.
*/
class TorcArgument
{
  public:
    TorcArgument()
      : m_value(QVariant()),
        m_helpText(""),
        m_exitImmediately(true),
        m_flags(TorcCommandLine::None)
    {
        // default constructor is provided for QHash support only
        LOG(VB_GENERAL, LOG_ERR, "Invalid TorcArgument usage");
    }

    TorcArgument(const QVariant &Default, const QString &HelpText, TorcCommandLine::Options Flags, bool Exit)
      : m_value(Default),
        m_helpText(HelpText),
        m_exitImmediately(Exit),
        m_flags(Flags)
    {
    }

    QVariant                 m_value;
    QString                  m_helpText;
    bool                     m_exitImmediately;
    TorcCommandLine::Options m_flags;
};

/*! \class TorcCommandLinePriv
 *  \brief Private implementation of command line handler.
 *
 * \todo Handle more QVariant types in Evaluate.
*/
class TorcCommandLinePriv
{
  public:
    TorcCommandLinePriv(TorcCommandLine::Options Flags);

    int         Evaluate       (int argc, const char * const *argv, bool &Exit);
    void        Add            (const QString Keys, const QVariant &Default, const QString &HelpText, TorcCommandLine::Options Flags = TorcCommandLine::None, bool ExitImmediately = false);
    QVariant    GetValue       (const QString &Key);

  private:
    QHash<QString,TorcArgument> m_options;
    QHash<QString,QString>      m_aliases;
    QHash<QString,QString>      m_help;
    uint                        m_maxLength;
};

TorcCommandLinePriv::TorcCommandLinePriv(TorcCommandLine::Options Flags)
  : m_maxLength(0)
{
    // always enable version, help and logging
    TorcCommandLine::Options options = Flags | TorcCommandLine::Version | TorcCommandLine::Help | TorcCommandLine::LogLevel | TorcCommandLine::LogType;

    if (options.testFlag(TorcCommandLine::Help))
        Add("h,help", QVariant(), "Display full usage information.", TorcCommandLine::Help, true);
    if (options.testFlag(TorcCommandLine::LogLevel))
        Add("l,log", QStringList("general"), "Set the logging level.", TorcCommandLine::None);
    if (options.testFlag(TorcCommandLine::LogType))
        Add("v,verbose,verbosity", QString("info"), "Set the logging type.", TorcCommandLine::None);
    if (options.testFlag(TorcCommandLine::Version))
        Add("version", QVariant(), "Display version information.", TorcCommandLine::Version, true);
    if (options.testFlag(TorcCommandLine::URI))
        Add("f,file,uri", QString(""), "URI pointing to the file/resource to be used.", TorcCommandLine::None);
}

/// \brief Add a command line option
void TorcCommandLinePriv::Add(const QString Keys, const QVariant &Default, const QString &HelpText, TorcCommandLine::Options Flags, bool ExitImmediately)
{
    QStringList keys = Keys.split(",");

    QStringList valid;

    bool first = true;
    QString master;
    foreach (QString key, keys)
    {
        if (key.contains("="))
        {
            std::cout << QString("Invalid option '%1'").arg(key).toLocal8Bit().constData() << std::endl;
        }
        else if (first && !m_options.contains(key))
        {
            master = key;
            m_options.insert(key, TorcArgument(Default, HelpText, Flags, ExitImmediately));
            valid.append("-" + key);
            first = false;
        }
        else if (!first && !m_aliases.contains(key) && !m_options.contains(key))
        {
            m_aliases.insert(key, master);
            valid.append("-" + key);
        }
        else
        {
            std::cout << QString("Command line option '%1' already in use - ignoring").arg(key).toLocal8Bit().constData() << std::endl;
        }
    }

    if (!valid.isEmpty())
    {
        QString options = valid.join(" OR ");
        m_help.insert(options, HelpText);
        if (options.size() > m_maxLength)
            m_maxLength = options.size() + 2;
    }
}

/// \brief Returns the value for the given key or an invalid QVariant if the option is not present.
QVariant TorcCommandLinePriv::GetValue(const QString &Key)
{
    if (m_options.contains(Key))
        return m_options[Key].m_value;

    if (m_aliases.contains(Key))
        return m_options[m_aliases[Key]].m_value;

    return QVariant();
}

/*! \brief Evaluate the command line paramaters
 *
 * Exit is set to true if any of the known options are discovered and require the application to exit
 * immediately (e.g. --help).
 *
 * Arguments can be preceeded by any number of '-'s. If a value is expected (the default value is a valid
 * QVariant) then the expected format is either --key=value or --key value.
 *
 * If there is a parsing error, the help text is printed and the application terminates immediately.
*/
int TorcCommandLinePriv::Evaluate(int argc, const char * const *argv, bool &Exit)
{
    QString error;
    bool parserror    = false;
    int  result       = GENERIC_EXIT_OK;
    bool printhelp    = false;
    bool printversion = false;

    // loop through the command line arguments
    for (int i = 1; i < argc; ++i)
    {
        QString key = QString::fromLocal8Bit(argv[i]);

        // remove trailing '-'s
        while (key.startsWith("-"))
            key = key.mid(1);

        QString value("");

        bool simpleformat = key.contains("=");
        if (simpleformat)
        {
            // option is --key=value format
            QStringList keyval = key.split("=");
            key = keyval.at(0).trimmed();
            value = keyval.at(1).trimmed();
        }

        // do we recognise this option
        if (!m_options.contains(key))
        {
            // is it an alias?
            if (!m_aliases.contains(key))
            {
                parserror = true;
                error = QString("Unknown command line option '%1'").arg(key);
                break;
            }
            else
            {
                key = m_aliases.value(key);
            }
        }

        TorcArgument& argument = m_options[key];

        // --key value format
        if (!simpleformat && argument.m_value.isValid())
        {
            if (i >= argc - 1)
            {
                parserror = true;
                error = QString("Insufficient arguments - option '%1' requires a value").arg(key);
                break;
            }

            value = QString::fromLocal8Bit(argv[++i]).trimmed();

            if (value.startsWith("-"))
            {
                parserror = true;
                error = QString("Option '%1' expects a value").arg(key);
                break;
            }
        }

        if (!argument.m_value.isValid())
        {
            if (!value.isEmpty())
            {
                // unexpected value
                parserror = true;
                error = QString("Option '%1' does not expect a value ('%2')").arg(key).arg(value);
                break;
            }
            else
            {
                // mark option as detected
                argument.m_value = QVariant((bool)true);
            }
        }
        else if (argument.m_value.isValid() && value.isEmpty())
        {
            parserror = true;
            error = QString("Option '%1' expects a value").arg(key).toLocal8Bit().constData();
            break;
        }

        Exit         |= argument.m_exitImmediately;
        printhelp    |= argument.m_flags & TorcCommandLine::Help;
        printversion |= argument.m_flags & TorcCommandLine::Version;

        // update the value for the option
        if (!value.isEmpty())
        {
            switch ((QMetaType::Type)argument.m_value.type())
            {
                case QMetaType::QStringList:
                    argument.m_value = QVariant(QStringList(value));
                    break;
                default:
                    argument.m_value = QVariant(value);
            }
        }
    }

    if (parserror)
    {
        result = GENERIC_EXIT_INVALID_CMDLINE;
        Exit = true;
        printhelp = true;
        std::cout << error.toLocal8Bit().constData() << std::endl << std::endl;
    }

    if (printhelp)
    {
        std::cout << "Torc Version : " << TORC_SOURCE_VERSION << std::endl;
        std::cout << "Command line options for " << QCoreApplication::applicationName().toLocal8Bit().constData() << ":" << std::endl << std::endl;

        QHash<QString,QString>::iterator it = m_help.begin();
        for ( ; it != m_help.end(); ++it)
        {
            QByteArray option(it.key().toLocal8Bit().constData());
            QByteArray padding(m_maxLength - option.size(), 32);
            std::cout << option.constData() << padding.constData() << it.value().toLocal8Bit().constData() << std::endl;
        }

        std::cout << std::endl << "All options may be preceeded by '-' or '--'" << std::endl;
    }

    if (printversion)
    {
        std::cout << "Torc Version : " << TORC_SOURCE_VERSION << std::endl;
        std::cout << "Torc Branch : "  << TORC_SOURCE_PATH << std::endl;
        std::cout << "QT Version : "   << QT_VERSION_STR << std::endl;
    }

    return GENERIC_EXIT_OK;
}

/*! \class TorcCommandLine
 *  \brief Public implementation of Torc command line handler.
 *
 * TorcCommandLine will always add handling for help, log, verbose and version handling.
 *
 * Additional pre-defined handlers can be implemented by passing additional TorcCommandLine::Options flags to
 * the constructor.
 *
 * Custom command line options can be implemented by calling Add and retrieving the expected value via GetValue.
*/
TorcCommandLine::TorcCommandLine(Options Flags)
  : m_priv(new TorcCommandLinePriv(Flags))
{
}

TorcCommandLine::~TorcCommandLine()
{
    delete m_priv;
}

/*! \brief Implement custom command line options.
 *
 * \param Keys            A comma separated list of synonymous command line options (e.g. "h,help").
 * \param Default         The default value AND type for an option (e.g. QString("info") or (bool)true).
 * \param HelpText        Brief help text for the option.
 * \param ExitImmediately Tell the application to exit immediately after processing the command line.
*/
void TorcCommandLine::Add(const QString Keys, const QVariant &Default, const QString &HelpText, bool ExitImmediately/*=false*/)
{
    m_priv->Add(Keys, Default, HelpText, TorcCommandLine::None, ExitImmediately);
}

/*! \brief Evaluate the command line options.
 *
 * \param Exit Will be set to true if the application should exit after command line processing.
*/
int TorcCommandLine::Evaluate(int argc, const char * const *argv, bool &Exit)
{
    return m_priv->Evaluate(argc, argv, Exit);
}

/*! \brief Return the value associated with Key or an invalid QVariant if the option is not present.
 *
 * \note In the case of options that require no value (e.g. --help), QVariant((bool)true) is returned if
 *       the option was detected.
*/
QVariant TorcCommandLine::GetValue(const QString &Key)
{
    return m_priv->GetValue(Key);
}
