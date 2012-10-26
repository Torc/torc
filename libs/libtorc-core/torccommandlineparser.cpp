/* -*- Mode: c++ -*-
*
* Copyright (C) Raymond Wagner 2011
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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

/*
* Class CommandLineArg
* Class TorcCommandLineParser
* Based on CommandLineParser by Raymond Wagner
*/

// Std
#include <signal.h>
#include <iostream>
#include <fstream>
#include <stdlib.h>

#ifndef _WIN32
#include <sys/ioctl.h>
#include <pwd.h>
#include <grp.h>
#if defined(__linux__) || defined(__LINUX__)
#include <sys/prctl.h>
#endif
#endif

using namespace std;

// Qt
#include <QtGlobal>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSize>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QString>
#include <QCoreApplication>
#include <QTextStream>
#include <QDateTime>

// Torc
#include "torccommandlineparser.h"
#include "torcexitcodes.h"
#include "torcconfig.h"
#include "torclogging.h"
#include "torcloggingimp.h"
#include "version.h"
#include "torccoreutils.h"

#define TERMWIDTH 79

const int kEnd          = 0,
          kEmpty        = 1,
          kOptOnly      = 2,
          kOptVal       = 3,
          kArg          = 4,
          kPassthrough  = 5,
          kInvalid      = 6;

const char* NamedOptType(int type);
bool OpenPidFile(ofstream &pidfs, const QString &pidfile);
bool SetUser(const QString &username);
int GetTermWidth(void);

/** \fn GetTermWidth(void)
 *  \brief returns terminal width, or 79 on error
 */
int GetTermWidth(void)
{
#ifdef _WIN32
    return TERMWIDTH;
#else
    struct winsize ws;

    if (ioctl(0, TIOCGWINSZ, &ws) != 0)
        return TERMWIDTH;

    return (int)ws.ws_col;
#endif
}

/** \fn NamedOptType
 *  \brief Return character string describing type of result from parser pass
 */
const char* NamedOptType(int type)
{
    switch (type)
    {
      case kEnd:
        return "kEnd";

      case kEmpty:
        return "kEmpty";

      case kOptOnly:
        return "kOptOnly";

      case kOptVal:
        return "kOptVal";

      case kArg:
        return "kArg";

      case kPassthrough:
        return "kPassthrough";

      case kInvalid:
        return "kInvalid";

      default:
        return "kUnknown";
    }
}

/** \class CommandLineArg
 *  \brief Definition for a single command line option
 *
 *  This class contains instructions for the command line parser about what
 *  options to process from the command line. Each instance can correspond
 *  to multiple argument keywords, and stores a default value, whether it
 *  has been supplied, help text, and optional interdependencies with other
 *  CommandLineArgs.
 */

/** \brief Default constructor for CommandLineArg class
 *
 *  This constructor is for use with command line parser, defining an option
 *  that can be used on the command line, and should be reported in --help
 *  printouts
 */
CommandLineArg::CommandLineArg(QString  Name, QVariant::Type Type,
                               QVariant Default,  QString HelpText,
                               QString  LongHelpText)
  : TorcReferenceCounter(), m_given(false), m_name(Name),
    m_group(""),            m_deprecated(""),
    m_removed(""),          m_removedversion(""),
    m_type(Type),           m_default(Default),
    m_helpText(HelpText),   m_longHelpText(LongHelpText)
{
}

/** \brief Reduced constructor for CommandLineArg class
 *
 *  This constructor is for internal use within the command line parser. It
 *  is intended for use in supplementary data storage for information not
 *  supplied directly on the command line.
 */
CommandLineArg::CommandLineArg(QString  Name, QVariant::Type Type,
                               QVariant Default)
  : TorcReferenceCounter(), m_given(false), m_name(Name),
    m_group(""),            m_deprecated(""),
    m_removed(""),          m_removedversion(""),
    m_type(Type),           m_default(Default)
{
}

/** \brief Dummy constructor for CommandLineArg class
 *
 *  This constructor is for internal use within the command line parser. It
 *  is used as a placeholder for defining relations between different command
 *  line arguments, and is reconciled with the proper argument of the same
 *  name prior to parsing inputs.
 */
CommandLineArg::CommandLineArg(QString Name)
  : TorcReferenceCounter(), m_given(false), m_name(Name),
    m_deprecated(""),       m_removed(""),
    m_removedversion(""),   m_type(QVariant::Invalid)
{
}

CommandLineArg::~CommandLineArg()
{
}

CommandLineArg* CommandLineArg::SetGroup(QString Group)
{
    m_group = Group;
    return this;
}

void CommandLineArg::AddKeyword(QString Keyword)
{
    m_keywords << Keyword;
}

/** \brief Return string containing all possible keyword triggers for this
 *         argument
 */
QString CommandLineArg::GetKeywordString(void) const
{
    return m_keywords.join(" OR ");
}

QString CommandLineArg::GetName(void) const
{
    return m_name;
}

/** \brief Return length of full keyword string for use in determining indent
 *         of help text
 */
int CommandLineArg::GetKeywordLength(void) const
{
    int len = GetKeywordString().length();

    QList<CommandLineArg*>::const_iterator it;
    for (it = m_parents.begin(); it != m_parents.end(); ++it)
        len = max(len, (*it)->GetKeywordLength()+2);

    return len;
}

/** \brief Return string containing help text with desired offset
 *
 *  This function returns a string containing all usable keywords and the
 *  shortened help text, for use with the general help printout showing all
 *  options. It automatically accounts for terminal width, and wraps the text
 *  accordingly.
 *
 *  The group option acts as a filter, only returning text if the argument is
 *  part of the group the parser is currently printing options for.
 *
 *  Child arguments will not produce help text on their own, but only indented
 *  beneath each of the marked parent arguments. The force option specifies
 *  that the function is being called by the parent argument, and help should
 *  be output.
 */
QString CommandLineArg::GetHelpString(int off, QString group, bool force) const
{
    QString helpstr;
    QTextStream msg(&helpstr, QIODevice::WriteOnly);
    int termwidth = GetTermWidth();

    if (m_helpText.isEmpty() && !force)
        // only print if there is a short help to print
        return helpstr;

    if ((m_group != group) && !force)
        // only print if looping over the correct group
        return helpstr;

    if (!m_parents.isEmpty() && !force)
        // only print if an independent option, not subject
        // to a parent option
        return helpstr;

    if (!m_deprecated.isEmpty())
        // option is marked as deprecated, do not show
        return helpstr;

    if (!m_removed.isEmpty())
        // option is marked as removed, do not show
        return helpstr;

    QString pad;
    pad.fill(' ', off);

    // print the first line with the available keywords
    QStringList hlist = m_helpText.split('\n');
    WrapList(hlist, termwidth-off);
    if (!m_parents.isEmpty())
        msg << "  ";
    msg << GetKeywordString().leftJustified(off, ' ')
        << hlist[0] << endl;

    // print remaining lines with necessary padding
    QStringList::const_iterator it;
    for (it = hlist.begin() + 1; it != hlist.end(); ++it)
        msg << pad << *it << endl;

    // loop through any child arguments to print underneath
    QList<CommandLineArg*>::const_iterator i2;
    for (i2 = m_children.begin(); i2 != m_children.end(); ++i2)
        msg << (*i2)->GetHelpString(off, group, true);

    msg.flush();
    return helpstr;
}

/** \brief Return string containing extended help text
 *
 *  This function returns a longer version of the help text than that provided
 *  with the list of arguments, intended for more detailed, specific
 *  information. This also documents the type of argument it takes, default
 *  value, and any relational dependencies with other arguments it might have.
 */
QString CommandLineArg::GetLongHelpString(QString Keyword) const
{
    QString helpstr;
    QTextStream msg(&helpstr, QIODevice::WriteOnly);
    int termwidth = GetTermWidth();

    // help called for an argument that is not me, this should not happen
    if (!m_keywords.contains(Keyword))
        return helpstr;

    // argument has been marked as removed, so warn user of such
    if (!m_removed.isEmpty())
        PrintRemovedWarning(Keyword);
    // argument has been marked as deprecated, so warn user of such
    else if (!m_deprecated.isEmpty())
        PrintDeprecatedWarning(Keyword);

    msg << "Option:      " << Keyword << endl << endl;

    bool first = true;

    // print all related keywords, padding for multiples
    QStringList::const_iterator it;
    for (it = m_keywords.begin(); it != m_keywords.end(); ++it)
    {
        if (*it != Keyword)
        {
            if (first)
            {
                msg << "Aliases:     " << *it << endl;
                first = false;
            }
            else
                msg << "             " << *it << endl;
        }
    }

    // print type and default for the stored value
    msg << "Type:        " << QVariant::typeToName(m_type) << endl;
    if (m_default.canConvert(QVariant::String))
        msg << "Default:     " << m_default.toString() << endl;

    QStringList help;
    if (m_longHelpText.isEmpty())
        help = m_helpText.split("\n");
    else
        help = m_longHelpText.split("\n");
    WrapList(help, termwidth - 13);

    // print description, wrapping and padding as necessary
    msg << "Description: " << help[0] << endl;
    for (it = help.begin() + 1; it != help.end(); ++it)
        msg << "             " << *it << endl;

    QList<CommandLineArg*>::const_iterator i2;

    // loop through the four relation types and print
    if (!m_parents.isEmpty())
    {
        msg << endl << "Can be used in combination with:" << endl;
        for (i2 = m_parents.constBegin(); i2 != m_parents.constEnd(); ++i2)
            msg << " " << (*i2)->GetPreferredKeyword()
                                    .toLocal8Bit().constData();
        msg << endl;
    }

    if (!m_children.isEmpty())
    {
        msg << endl << "Allows the use of:" << endl;
        for (i2 = m_children.constBegin(); i2 != m_children.constEnd(); ++i2)
            msg << " " << (*i2)->GetPreferredKeyword()
                                    .toLocal8Bit().constData();
        msg << endl;
    }

    if (!m_requires.isEmpty())
    {
        msg << endl << "Requires the use of:" << endl;
        for (i2 = m_requires.constBegin(); i2 != m_requires.constEnd(); ++i2)
            msg << " " << (*i2)->GetPreferredKeyword()
                                    .toLocal8Bit().constData();
        msg << endl;
    }

    if (!m_blocks.isEmpty())
    {
        msg << endl << "Prevents the use of:" << endl;
        for (i2 = m_blocks.constBegin(); i2 != m_blocks.constEnd(); ++i2)
            msg << " " << (*i2)->GetPreferredKeyword()
                                    .toLocal8Bit().constData();
        msg << endl;
    }

    msg.flush();
    return helpstr;
}

/** \brief Set option as provided on command line with no value
 *
 *  This specifies that an option is given, but there is no corresponding
 *  value, meaning this can only be used on a boolean, integer, and string
 *  arguments. All other will return false.
 */
bool CommandLineArg::Set(QString Option)
{
    m_usedKeyword = Option;

    switch (m_type)
    {
      case QVariant::Bool:
        m_stored = QVariant(!m_default.toBool());
        break;

      case QVariant::Int:
        if (m_stored.isNull())
            m_stored = QVariant(1);
        else
            m_stored = QVariant(m_stored.toInt() + 1);
        break;

      case QVariant::String:
        m_stored = m_default;
        break;

      default:
        cerr << "Command line option did not receive value:" << endl
             << "    " << Option.toLocal8Bit().constData() << endl;
        return false;
    }

    m_given = true;
    return true;
}

bool CommandLineArg::Set(QString Option, QByteArray Value)
{
    QVariantList vlist;
    QList<QByteArray> blist;
    QVariantMap vmap;
    m_usedKeyword = Option;

    switch (m_type)
    {
      case QVariant::Bool:
        cerr << "Boolean type options do not accept values:" << endl
             << "    " << Option.toLocal8Bit().constData() << endl;
        return false;

      case QVariant::String:
        m_stored = QVariant(Value);
        break;

      case QVariant::Int:
        m_stored = QVariant(Value.toInt());
        break;

      case QVariant::UInt:
        m_stored = QVariant(Value.toUInt());
        break;

      case QVariant::LongLong:
        m_stored = QVariant(Value.toLongLong());
        break;

      case QVariant::Double:
        m_stored = QVariant(Value.toDouble());
        break;

      case QVariant::DateTime:
        m_stored = QVariant(TorcDateTimeFromString(QString(Value)));
        break;

      case QVariant::StringList:
        if (!m_stored.isNull())
            vlist = m_stored.toList();
        vlist << Value;
        m_stored = QVariant(vlist);
        break;

      case QVariant::Map:
        if (!Value.contains('='))
        {
            cerr << "Command line option did not get expected "
                 << "key/value pair" << endl;
            return false;
        }

        blist = Value.split('=');

        if (!m_stored.isNull())
            vmap = m_stored.toMap();
        vmap[QString(blist[0])] = QVariant(blist[1]);
        m_stored = QVariant(vmap);
        break;

      case QVariant::Size:
        if (!Value.contains('x'))
        {
            cerr << "Command line option did not get expected "
                 << "XxY pair" << endl;
            return false;
        }

        blist = Value.split('x');
        m_stored = QVariant(QSize(blist[0].toInt(), blist[1].toInt()));
        break;

      default:
        m_stored = QVariant(Value);
    }

    m_given = true;
    return true;
}

void CommandLineArg::Set(QVariant Value)
{
    m_stored = Value;
    m_given  = true;
}

CommandLineArg* CommandLineArg::SetParentOf(QString Child)
{
    m_children << new CommandLineArg(Child);
    return this;
}

CommandLineArg* CommandLineArg::SetParentOf(QStringList Children)
{
    QStringList::const_iterator it = Children.begin();
    for (; it != Children.end(); ++it)
        m_children << new CommandLineArg(*it);
    return this;
}

CommandLineArg* CommandLineArg::SetParent(QString Child)
{
    m_parents << new CommandLineArg(Child);
    return this;
}

CommandLineArg* CommandLineArg::SetParent(QStringList Children)
{
    QStringList::const_iterator it = Children.begin();
    for (; it != Children.end(); ++it)
        m_parents << new CommandLineArg(*it);
    return this;
}

CommandLineArg* CommandLineArg::SetChildOf(QString Parent)
{
    m_parents << new CommandLineArg(Parent);
    return this;
}

CommandLineArg* CommandLineArg::SetChildOf(QStringList Parents)
{
    QStringList::const_iterator it = Parents.begin();
    for (; it != Parents.end(); ++it)
        m_parents << new CommandLineArg(*it);
    return this;
}

CommandLineArg* CommandLineArg::SetChild(QString opt)
{
    m_children << new CommandLineArg(opt);
    return this;
}

/** \brief Set argument as parent of multiple children
 */
CommandLineArg* CommandLineArg::SetChild(QStringList opts)
{
    QStringList::const_iterator i = opts.begin();
    for (; i != opts.end(); ++i)
        m_children << new CommandLineArg(*i);
    return this;
}

/** \brief Set argument as parent of given child and mark as required
 */
CommandLineArg* CommandLineArg::SetRequiredChild(QString opt)
{
    m_children << new CommandLineArg(opt);
    m_requires << new CommandLineArg(opt);
    return this;
}

/** \brief Set argument as parent of multiple children and mark as required
 */
CommandLineArg* CommandLineArg::SetRequiredChild(QStringList opts)
{
    QStringList::const_iterator i = opts.begin();
    for (; i != opts.end(); ++i)
    {
        m_children << new CommandLineArg(*i);
        m_requires << new CommandLineArg(*i);
    }
    return this;
}

/** \brief Set argument as child required by given parent
 */
CommandLineArg* CommandLineArg::SetRequiredChildOf(QString opt)
{
    m_parents << new CommandLineArg(opt);
    m_requiredby << new CommandLineArg(opt);
    return this;
}

/** \brief Set argument as child required by multiple parents
 */
CommandLineArg* CommandLineArg::SetRequiredChildOf(QStringList opts)
{
    QStringList::const_iterator i = opts.begin();
    for (; i != opts.end(); ++i)
    {
        m_parents << new CommandLineArg(*i);
        m_requiredby << new CommandLineArg(*i);
    }
    return this;
}

/** \brief Set argument as requiring given option
 */
CommandLineArg* CommandLineArg::SetRequires(QString Required)
{
    m_requires << new CommandLineArg(Required);
    return this;
}

/** \brief Set argument as requiring multiple options
 */
CommandLineArg* CommandLineArg::SetRequires(QStringList Required)
{
    QStringList::const_iterator it = Required.begin();
    for (; it != Required.end(); ++it)
        m_requires << new CommandLineArg(*it);
    return this;
}

/** \brief Set argument as incompatible with given option
 */
CommandLineArg* CommandLineArg::SetBlocks(QString opt)
{
    m_blocks << new CommandLineArg(opt);
    return this;
}

/** \brief Set argument as incompatible with multiple options
 */
CommandLineArg* CommandLineArg::SetBlocks(QStringList opts)
{
    QStringList::const_iterator i = opts.begin();
    for (; i != opts.end(); ++i)
        m_blocks << new CommandLineArg(*i);
    return this;
}

/** \brief Set option as deprecated
 */
CommandLineArg* CommandLineArg::SetDeprecated(QString Deprecated)
{
    if (Deprecated.isEmpty())
        Deprecated = "and will be removed in a future version.";
    m_deprecated = Deprecated;
    return this;
}

CommandLineArg* CommandLineArg::SetRemoved(QString Removed, QString Version)
{
    if (Removed.isEmpty())
        Removed = "and is no longer available in this version.";
    m_removed = Removed;
    m_removedversion = Version;
    return this;
}

void CommandLineArg::SetParentOf(CommandLineArg *Child, bool Forward)
{
    Child->UpRef();

    bool replaced = false;
    for (int i = 0; i < m_children.size(); i++)
    {
        if (m_children[i]->m_name == Child->m_name)
        {
            m_children[i]->DownRef();
            m_children.replace(i, Child);
            replaced = true;
            break;
        }
    }

    if (!replaced)
        m_children << Child;

    if (Forward)
        Child->SetChildOf(this, false);
}

void CommandLineArg::SetChildOf(CommandLineArg *Parent, bool Forward)
{
    Parent->UpRef();

    bool replaced = false;
    for (int i = 0; i < m_parents.size(); i++)
    {
        if (m_parents[i]->m_name == Parent->m_name)
        {
            m_parents[i]->DownRef();
            m_parents.replace(i, Parent);
            replaced = true;
            break;
        }
    }

    if (!replaced)
        m_parents << Parent;

    if (Forward)
        Parent->SetParentOf(this, false);
}

void CommandLineArg::SetRequires(CommandLineArg *Required, bool Forward)
{
    Required->UpRef();

    bool replaced = false;
    for (int i = 0; i < m_requires.size(); i++)
    {
        if (m_requires[i]->m_name == Required->m_name)
        {
            m_requires[i]->DownRef();
            m_requires.replace(i, Required);
            replaced = true;
            break;
        }
    }

    if (!replaced)
        m_requires << Required;
}

void CommandLineArg::SetBlocks(CommandLineArg *Blocks, bool Forward)
{
    Blocks->UpRef();

    bool replaced = false;
    for (int i = 0; i < m_blocks.size(); i++)
    {
        if (m_blocks[i]->m_name == Blocks->m_name)
        {
            m_blocks[i]->DownRef();
            m_blocks.replace(i, Blocks);
            replaced = true;
            break;
        }
    }

    if (!replaced)
        m_blocks << Blocks;

    if (Forward)
        Blocks->SetBlocks(this, false);
}

/** \brief Mark a list of arguments as mutually exclusive
 */
void CommandLineArg::AllowOneOf(QList<CommandLineArg*> Arguments)
{
    QList<CommandLineArg*>::const_iterator it,i2;

    // loop through all but the last entry
    for (it = Arguments.begin(); it != Arguments.end()-1; ++it)
    {
        // loop through the next to the last entry
        // and block use with the current
        for (i2 = it+1; i2 != Arguments.end(); ++i2)
        {
            (*it)->SetBlocks(*i2);
        }

        if ((*it)->m_type == QVariant::Invalid)
            (*it)->DownRef();
    }
}

QString CommandLineArg::GetPreferredKeyword(void) const
{
    QStringList::const_iterator it;
    QString preferred;
    int len = 0, len2;

    for (it = m_keywords.constBegin(); it != m_keywords.constEnd(); ++it)
    {
        len2 = (*it).size();
        if (len2 > len)
        {
            preferred = *it;
            len = len2;
        }
    }

    return preferred;
}

/** \brief Test all related arguments to make sure specified requirements are
 *         fulfilled
 */
bool CommandLineArg::TestLinks(void) const
{
    if (!m_given)
        return true; // not in use, no need for checks

    QList<CommandLineArg*>::const_iterator i;

    bool passes = false;
    for (i = m_parents.constBegin(); i != m_parents.constEnd(); ++i)
    {
        // one of these must have been defined
        if ((*i)->m_given)
        {
            passes = true;
            break;
        }
    }
    if (!passes && !m_parents.isEmpty())
    {
        cerr << "ERROR: " << m_usedKeyword.toLocal8Bit().constData()
             << " requires at least one of the following arguments" << endl;
        for (i = m_parents.constBegin(); i != m_parents.constEnd(); ++i)
            cerr << " "
                 << (*i)->GetPreferredKeyword().toLocal8Bit().constData();
        cerr << endl << endl;
        return false;
    }

    // we dont care about children

    for (i = m_requires.constBegin(); i != m_requires.constEnd(); ++i)
    {
        // all of these must have been defined
        if (!(*i)->m_given)
        {
            cerr << "ERROR: " << m_usedKeyword.toLocal8Bit().constData()
                 << " requires all of the following be defined as well"
                 << endl;
            for (i = m_requires.constBegin(); i != m_requires.constEnd(); ++i)
                cerr << " "
                     << (*i)->GetPreferredKeyword().toLocal8Bit()
                                                   .constData();
            cerr << endl << endl;
            return false;
        }
    }

    for (i = m_blocks.constBegin(); i != m_blocks.constEnd(); ++i)
    {
        // none of these can be defined
        if ((*i)->m_given)
        {
            cerr << "ERROR: " << m_usedKeyword.toLocal8Bit().constData()
                 << " requires that none of the following be defined" << endl;
            for (i = m_blocks.constBegin(); i != m_blocks.constEnd(); ++i)
                cerr << " "
                     << (*i)->GetPreferredKeyword().toLocal8Bit()
                                                   .constData();
            cerr << endl << endl;
            return false;
        }
    }

    return true;
}

/** \brief Clear out references to other arguments in preparation for deletion
 */
void CommandLineArg::CleanupLinks(void)
{
    // clear out interdependent pointers in preparation for deletion
    while (!m_parents.isEmpty())
        m_parents.takeFirst()->DownRef();

    while (!m_children.isEmpty())
        m_children.takeFirst()->DownRef();

    while (!m_blocks.isEmpty())
        m_blocks.takeFirst()->DownRef();

    while (!m_requires.isEmpty())
        m_requires.takeFirst()->DownRef();

    while (!m_requiredby.isEmpty())
        m_requiredby.takeFirst()->DownRef();
}

void CommandLineArg::WrapList(QStringList &List, int Width)
{
    for(int i = 0; i < List.size(); i++)
    {
        QString string = List.at(i);

        if(string.size() <= Width)
            continue;

        QString left  = string.left(Width);
        bool inserted = false;

        while(!inserted && left.right(1) != " ")
        {
            if(string.mid(left.size(), 1) == " ")
            {
                List.replace(i, left);
                List.insert(i+1, string.mid(left.size()).trimmed());
                inserted = true;
            }
            else
            {
                left.chop(1);
                if(!left.contains(" "))
                {
                    // Line is too long, just hyphenate it
                    List.replace(i, left + "-");
                    List.insert(i+1, string.mid(left.size()));
                    inserted = true;
                }
            }
        }

        if(!inserted)
        {
            left.chop(1);
            List.replace(i, left);
            List.insert(i+1, string.mid(left.size()).trimmed());
        }
    }
}

/** \brief Internal use. Print processed input in verbose mode.
 */
void CommandLineArg::PrintVerbose(void) const
{
    if (!m_given)
        return;

    cerr << "  " << m_name.leftJustified(30).toLocal8Bit().constData();

    QSize tmpsize;
    QMap<QString, QVariant> tmpmap;
    QMap<QString, QVariant>::const_iterator it;
    QVariantList vlist;
    QVariantList::const_iterator it2;
    bool first;

    switch (m_type)
    {
      case QVariant::Bool:
        cerr << (m_stored.toBool() ? "True" : "False") << endl;
        break;

      case QVariant::Int:
        cerr << m_stored.toInt() << endl;
        break;

      case QVariant::UInt:
        cerr << m_stored.toUInt() << endl;
        break;

      case QVariant::LongLong:
        cerr << m_stored.toLongLong() << endl;
        break;

      case QVariant::Double:
        cerr << m_stored.toDouble() << endl;
        break;

      case QVariant::Size:
        tmpsize = m_stored.toSize();
        cerr <<  "x=" << tmpsize.width()
             << " y=" << tmpsize.height()
             << endl;
        break;

      case QVariant::String:
        cerr << '"' << m_stored.toByteArray().constData()
             << '"' << endl;
        break;

      case QVariant::StringList:
        vlist = m_stored.toList();
        it2 = vlist.begin();
        cerr << '"' << it2->toByteArray().constData() << '"';
        ++it2;
        for (; it2 != vlist.end(); ++it2)
            cerr << ", \""
                 << it2->constData()
                 << '"';
        cerr << endl;
        break;

      case QVariant::Map:
        tmpmap = m_stored.toMap();
        first = true;

        for (it = tmpmap.begin(); it != tmpmap.end(); ++it)
        {
            if (first)
                first = false;
            else
                cerr << QString("").leftJustified(32)
                                   .toLocal8Bit().constData();

            cerr << it.key().toLocal8Bit().constData()
                 << '='
                 << it->toByteArray().constData()
                 << endl;
        }

        break;

      case QVariant::DateTime:
        cerr << m_stored.toDateTime().toString(Qt::ISODate)
                        .toLocal8Bit().constData()
             << endl;
        break;

      default:
        cerr << endl;
    }
}

/** \brief Internal use. Print warning for removed option.
 */
void CommandLineArg::PrintRemovedWarning(QString &Keyword) const
{
    QString warn = QString("%1 has been removed").arg(Keyword);
    if (!m_removedversion.isEmpty())
        warn += QString(" as of Torc %1").arg(m_removedversion);

    cerr << QString("****************************************************\n"
                    " WARNING: %1\n"
                    "          %2\n"
                    "****************************************************\n\n")
                .arg(warn).arg(m_removed)
                .toLocal8Bit().constData();
}

/** \brief Internal use. Print warning for deprecated option.
 */
void CommandLineArg::PrintDeprecatedWarning(QString &Keyword) const
{
    cerr << QString("****************************************************\n"
                    " WARNING: %1 has been deprecated\n"
                    "          %2\n"
                    "****************************************************\n\n")
                .arg(Keyword).arg(m_deprecated)
                .toLocal8Bit().constData();
}

/** \class TorcCommandLineParser
 *  \brief Parent class for defining application command line parsers
 *
 *  This class provides a generic interface for defining and parsing available
 *  command line options. Options can be provided manually using the Add()
 *  method, or one of several canned Add*() methods. Once defined, the command
 *  line is parsed using the Parse() method, and results are available through
 *  Qt standard to<Type>() methods.
 */

/** \brief Default constructor for MythCommandLineArg class
 */
TorcCommandLineParser::TorcCommandLineParser() :
    m_appname(""),
    m_passthroughActive(false),
    m_overridesImported(false),
    m_verbose(false)
{
    m_appname = QCoreApplication::applicationName();

    if (!qgetenv("VERBOSE_PARSER").isEmpty())
    {
        cerr << "TorcCommandLineParser is now operating verbosely." << endl;
        m_verbose = true;
    }

    LoadArguments();
}

TorcCommandLineParser::~TorcCommandLineParser()
{
    QMap<QString, CommandLineArg*>::iterator i;

    i = m_namedArgs.begin();
    while (i != m_namedArgs.end())
    {
        (*i)->CleanupLinks();
        (*i)->DownRef();
        i = m_namedArgs.erase(i);
    }

    i = m_optionedArgs.begin();
    while (i != m_optionedArgs.end())
    {
        (*i)->DownRef();
        i = m_optionedArgs.erase(i);
    }
}

/** \brief Add a new command line argument
 *
 *  This is the primary method for adding new arguments for processing. There
 *  are several overloaded convenience methods that tie into this, allowing
 *  it to be called with fewer inputs.
 *
 *  Args     - list of arguments to allow use of on the command line
 *  Name     - internal name to be used when pulling processed data out
 *  Type     - type of variable to be processed
 *  Default  - default value to provide if one is not supplied or option is
 *             not used
 *  HelpText - short help text, displayed when printing all available options
 *             with '--help'
 *             if this is empty, the argument will not be shown
 *  LongHelpText
             - extended help text, displayed when help about a specific option
 *             is requested using '--help <option>'
 *
 *  Allowed types are:
 *    Bool          - set to value, or default if value is not provided
 *    String        - set to value, or default if value is not provided
 *    Int           - set to value, or behaves as counter for multiple uses if
 *                    value is not provided
 *    UInt
 *    LongLong
 *    Double
 *    DateTime      - accepts ISO8601 and Torc's flattened version
 *    StringList    - accepts multiple uses, appended as individual strings
 *    Map           - accepts multiple pairs, in the syntax "key=value"
 *    Size          - accepts size in the syntax "XxY"
 */
CommandLineArg* TorcCommandLineParser::Add(QStringList Args,
        QString Name, QVariant::Type Type, QVariant Default,
        QString HelpText, QString LongHelpText)
{
    CommandLineArg *arg;

    if (m_namedArgs.contains(Name))
        arg = m_namedArgs[Name];
    else
    {
        arg = new CommandLineArg(Name, Type, Default, HelpText, LongHelpText);
        m_namedArgs.insert(Name, arg);
    }

    QStringList::const_iterator it;
    for (it = Args.begin(); it != Args.end(); ++it)
    {
        if (!m_optionedArgs.contains(*it))
        {
            arg->AddKeyword(*it);
            if (m_verbose)
                cerr << "Adding " << (*it).toLocal8Bit().constData()
                     << " as taking type '" << QVariant::typeToName(Type)
                     << "'" << endl;
            arg->UpRef();
            m_optionedArgs.insert(*it, arg);
        }
    }

    return arg;
}

/** \brief Print application version information
 */
void TorcCommandLineParser::PrintVersion(void) const
{
    cout << "Please attach all output as a file in bug reports." << endl;
    cout << "Torc Version : " << TORC_SOURCE_VERSION << endl;
    cout << "Torc Branch : " << TORC_SOURCE_PATH << endl;
    //cout << "Network Protocol : " << TORC_PROTO_VERSION << endl;
    //cout << "Library API : " << TORC_BINARY_VERSION << endl;
    cout << "QT Version : " << QT_VERSION_STR << endl;
#ifdef TORC_BUILD_CONFIG
    cout << "Options compiled in:" <<endl;
    cout << TORC_BUILD_CONFIG << endl;
#endif
}

/** \brief Print command line option help
 */
void TorcCommandLineParser::PrintHelp(void) const
{
    QString help = GetHelpString();
    cerr << help.toLocal8Bit().constData();
}

/** \brief Generate command line option help text
 *
 *  Generates generic help or specific help, depending on whether a value
 *  was provided to the --help option
 */
QString TorcCommandLineParser::GetHelpString(void) const
{
    QString helpstr;
    QTextStream msg(&helpstr, QIODevice::WriteOnly);

    QString versionStr = QString("%1 version: %2 [%3] www.torcdvr.org")
        .arg(m_appname).arg(TORC_SOURCE_PATH).arg(TORC_SOURCE_VERSION);
    msg << versionStr << endl;

    if (ToString("showhelp").isEmpty())
    {
        // build generic help text

        QString descr = GetHelpHeader();
        if (descr.size() > 0)
            msg << endl << descr << endl << endl;

        // loop through registered arguments to populate list of groups
        QStringList groups("");
        int maxlen = 0;
        QMap<QString, CommandLineArg*>::const_iterator it;
        for (it = m_namedArgs.begin(); it != m_namedArgs.end(); ++it)
        {
            maxlen = max((*it)->GetKeywordLength(), maxlen);
            if (!groups.contains((*it)->m_group))
                groups << (*it)->m_group;
        }

        // loop through list of groups and print help string for each
        // arguments will filter themselves if they are not in the group
        maxlen += 4;
        QStringList::const_iterator i2;
        for (i2 = groups.begin(); i2 != groups.end(); ++i2)
        {
            if ((*i2).isEmpty())
                msg << "Misc. Options:" << endl;
            else
                msg << (*i2).toLocal8Bit().constData() << " Options:" << endl;

            for (it = m_namedArgs.begin(); it != m_namedArgs.end(); ++it)
                msg << (*it)->GetHelpString(maxlen, *i2);
            msg << endl;
        }
    }
    else
    {
        // build help for a specific argument
        QString optstr = "-" + ToString("showhelp");
        if (!m_optionedArgs.contains(optstr))
        {
            optstr = "-" + optstr;
            if (!m_optionedArgs.contains(optstr))
                return QString("Could not find option matching '%1'\n")
                            .arg(ToString("showhelp"));
        }

        msg << m_optionedArgs[optstr]->GetLongHelpString(optstr);
    }

    msg.flush();
    return helpstr;
}

QString TorcCommandLineParser::GetHelpHeader(void) const
{
    return QString("");
}

/** \brief Internal use. Pull next key/value pair from argv.
 */
int TorcCommandLineParser::GetOpt(int argc, const char * const * argv,
                                  int &argpos, QString &opt,
                                  QByteArray &Value)
{
    opt.clear();
    Value.clear();

    if (argpos >= argc)
        // this shouldnt happen, return and exit
        return kEnd;

    QByteArray tmp(argv[argpos]);
    if (tmp.isEmpty())
        // string is empty, return and loop
        return kEmpty;

    if (m_passthroughActive)
    {
        // pass through has been activated
        Value = tmp;
        return kArg;
    }

    if (tmp.startsWith('-') && tmp.size() > 1)
    {
        if (tmp == "--")
        {
            // all options beyond this will be passed as a single string
            m_passthroughActive = true;
            return kPassthrough;
        }

        if (tmp.contains('='))
        {
            // option contains '=', split
            QList<QByteArray> blist = tmp.split('=');

            if (blist.size() != 2)
            {
                // more than one '=' in option, this is not handled
                opt = QString(tmp);
                return kInvalid;
            }

            opt = QString(blist[0]);
            Value = blist[1];
            return kOptVal;
        }

        opt = QString(tmp);

        if (argpos+1 >= argc)
            // end of input, option only
            return kOptOnly;

        tmp = QByteArray(argv[++argpos]);
        if (tmp.isEmpty())
            // empty string, option only
            return kOptOnly;

        if (tmp.startsWith("-") && tmp.size() > 1)
        {
            // no value found for option, backtrack
            argpos--;
            return kOptOnly;
        }

        Value = tmp;
        return kOptVal;
    }
    else
    {
        // input is not an option string, return as arg
        Value = tmp;
        return kArg;
    }

}

/** \brief Loop through argv and populate arguments with values
 *
 *  This should not be called until all arguments are added to the parser.
 *  This returns false if the parser hits an argument it is not designed
 *  to handle.
 */
bool TorcCommandLineParser::Parse(int argc, const char * const * argv,
                                  bool &JustExit)
{
    int res;
    QString opt;
    QByteArray val;
    CommandLineArg *argdef;

    // reconnect interdependencies between command line options
    if (!ReconcileLinks())
        return false;

    // loop through command line arguments until all are spent
    for (int argpos = 1; argpos < argc; ++argpos)
    {

        // pull next option
        res = GetOpt(argc, argv, argpos, opt, val);

        if (m_verbose)
            cerr << "res: " << NamedOptType(res) << endl
                 << "opt:  " << opt.toLocal8Bit().constData() << endl
                 << "val:  " << val.constData() << endl << endl;

        // '--' found on command line, enable passthrough mode
        if (res == kPassthrough && !m_namedArgs.contains("_passthrough"))
        {
            cerr << "Received '--' but passthrough has not been enabled" << endl;
            SetValue("showhelp", "");
            return false;
        }

        // end of options found, terminate loop
        if (res == kEnd)
            break;

        // GetOpt pulled an empty option, this shouldnt happen by ignore
        // it and continue
        else if (res == kEmpty)
            continue;

        // more than one equal found in key/value pair, fault out
        else if (res == kInvalid)
        {
            cerr << "Invalid option received:" << endl << "    "
                 << opt.toLocal8Bit().constData();
            SetValue("showhelp", "");
            return false;
        }

        // passthrough is active, so add the data to the stringlist
        else if (m_passthroughActive)
        {
            m_namedArgs["_passthrough"]->Set("", val);
            continue;
        }

        // argument with no preceeding '-' encountered, add to stringlist
        else if (res == kArg)
        {
            if (!m_namedArgs.contains("_args"))
            {
                cerr << "Received '"
                     << val.constData()
                     << "' but unassociated arguments have not been enabled"
                     << endl;
                SetValue("showhelp", "");
                return false;
            }

            m_namedArgs["_args"]->Set("", val);
            continue;
        }

        // this line should not be passed once arguments have started collecting
        if (ToBool("_args"))
        {
            cerr << "Command line arguments received out of sequence"
                 << endl;
            SetValue("showhelp", "");
            return false;
        }

#ifdef Q_OS_MAC
        if (opt.startsWith("-psn_"))
        {
            cerr << "Ignoring Process Serial Number from command line"
                 << endl;
            continue;
        }
#endif

        if (!m_optionedArgs.contains(opt))
        {
            // argument is unhandled, check if parser allows arbitrary input
            if (m_namedArgs.contains("_extra"))
            {
                // arbitrary allowed, specify general collection pool
                argdef = m_namedArgs["_extra"];
                QByteArray tmp = opt.toLocal8Bit();
                tmp += '=';
                tmp += val;
                val = tmp;
                res = kOptVal;
            }
            else
            {
                // arbitrary not allowed, fault out
                cerr << "Unhandled option given on command line:" << endl
                     << "    " << opt.toLocal8Bit().constData() << endl;
                SetValue("showhelp", "");
                return false;
            }
        }
        else
            argdef = m_optionedArgs[opt];

        // argument has been marked as removed, warn user and fail
        if (!argdef->m_removed.isEmpty())
        {
            argdef->PrintRemovedWarning(opt);
            SetValue("showhelp", "");
            return false;
        }

        // argument has been marked as deprecated, warn user
        if (!argdef->m_deprecated.isEmpty())
            argdef->PrintDeprecatedWarning(opt);

        if (m_verbose)
            cerr << "name: " << argdef->GetName().toLocal8Bit().constData()
                 << endl;

        // argument is keyword only, no value
        if (res == kOptOnly)
        {
            if (!argdef->Set(opt))
            {
                SetValue("showhelp", "");
                return false;
            }
        }
        // argument has keyword and value
        else if (res == kOptVal)
        {
            if (!argdef->Set(opt, val))
            {
                // try testing keyword with no value
                if (!argdef->Set(opt))
                {
                    SetValue("showhelp", "");
                    return false;
                }
                // drop back an iteration so the unused value will get
                // processed a second time as a keyword-less argument
                --argpos;
            }
        }
        else
        {
            SetValue("showhelp", "");
            return false; // this should not occur
        }

        if (m_verbose)
            cerr << "value: " << argdef->m_stored.toString().toLocal8Bit().constData()
                 << endl;
    }

    QMap<QString, CommandLineArg*>::const_iterator it;

    if (m_verbose)
    {
        cerr << "Processed option list:" << endl;
        for (it = m_namedArgs.begin(); it != m_namedArgs.end(); ++it)
            (*it)->PrintVerbose();

        if (m_namedArgs.contains("_args"))
        {
            cerr << endl << "Extra argument list:" << endl;
            QStringList slist = ToStringList("_args");
            QStringList::const_iterator it2 = slist.begin();
            for (; it2 != slist.end(); ++it2)
                cerr << "  " << (*it2).toLocal8Bit().constData() << endl;
        }

        if (m_namedArgs.contains("_passthrough"))
        {
            cerr << endl << "Passthrough string:" << endl;
            cerr << "  " << GetPassthrough().toLocal8Bit().constData() << endl;
        }

        cerr << endl;
    }

    // make sure all interdependencies are fulfilled
    for (it = m_namedArgs.begin(); it != m_namedArgs.end(); ++it)
    {
        if (!(*it)->TestLinks())
        {
            QString keyword = (*it)->m_usedKeyword;
            if (keyword.startsWith('-'))
            {
                if (keyword.startsWith("--"))
                    keyword.remove(0,2);
                else
                    keyword.remove(0,1);
            }

            SetValue("showhelp", keyword);
            return false;
        }
    }

    if (ToBool("showhelp"))
    {
        PrintHelp();
        JustExit = true;
    }

    if (ToBool("showversion"))
    {
        PrintVersion();
        JustExit = true;
    }

    return true;
}

CommandLineArg* TorcCommandLineParser::Add(QString Arg,  QString Name,
                                           bool Default, QString HelpText,
                                           QString LongHelpText)
{
    return Add(QStringList(Arg), Name, QVariant::Bool,
               QVariant(Default), HelpText, LongHelpText);
}

CommandLineArg* TorcCommandLineParser::Add(QString Arg, QString Name,
                                           int Default, QString HelpText,
                                           QString LongHelpText)
{
    return Add(QStringList(Arg), Name, QVariant::Int,
               QVariant(Default), HelpText, LongHelpText);
}

CommandLineArg* TorcCommandLineParser::Add(QString Arg, QString Name,
                                           uint Default, QString HelpText,
                                           QString LongHelpText)
{
    return Add(QStringList(Arg), Name, QVariant::UInt,
               QVariant(Default), HelpText, LongHelpText);
}

CommandLineArg* TorcCommandLineParser::Add(QString Arg, QString Name,
                                           long long Default, QString HelpText,
                                           QString LongHelpText)
{
    return Add(QStringList(Arg), Name, QVariant::LongLong,
               QVariant(Default), HelpText, LongHelpText);
}

CommandLineArg* TorcCommandLineParser::Add(QString Arg, QString Name,
                                           double Default, QString HelpText,
                                           QString LongHelpText)
{
    return Add(QStringList(Arg), Name, QVariant::Double,
               QVariant(Default), HelpText, LongHelpText);
}

CommandLineArg* TorcCommandLineParser::Add(QString Arg, QString Name,
                                           QString Default, QString HelpText,
                                           QString LongHelpText)
{
    return Add(QStringList(Arg), Name, QVariant::String,
               QVariant(Default), HelpText, LongHelpText);
}

CommandLineArg* TorcCommandLineParser::Add(QString Arg, QString Name,
                                           const char *Default, QString HelpText,
                                           QString LongHelpText)
{
    return Add(QStringList(Arg), Name, QVariant::String,
               QVariant(Default), HelpText, LongHelpText);
}

CommandLineArg* TorcCommandLineParser::Add(QString Arg, QString Name,
                                           QSize Default, QString HelpText,
                                           QString LongHelpText)
{
    return Add(QStringList(Arg), Name, QVariant::Size,
               QVariant(Default), HelpText, LongHelpText);
}

CommandLineArg* TorcCommandLineParser::Add(QString Arg, QString Name,
                                           QDateTime Default, QString HelpText,
                                           QString LongHelpText)
{
    return Add(QStringList(Arg), Name, QVariant::DateTime,
               QVariant(Default), HelpText, LongHelpText);
}

CommandLineArg* TorcCommandLineParser::Add(QString Arg, QString Name,
                                           QVariant::Type Type, QString HelpText,
                                           QString LongHelpText)
{
    return Add(QStringList(Arg), Name, Type,
               QVariant(Type), HelpText, LongHelpText);
}

CommandLineArg* TorcCommandLineParser::Add(QString Arg, QString Name,
                                           QVariant::Type Type, QVariant Default,
                                           QString HelpText, QString LongHelpText)
{
    return Add(QStringList(Arg), Name, Type,
               Default, HelpText, LongHelpText);
}

CommandLineArg* TorcCommandLineParser::Add(QStringList Args, QString Name,
                                           bool Default, QString HelpText,
                                           QString LongHelpText)
{
    return Add(Args, Name, QVariant::Bool,
               QVariant(Default), HelpText, LongHelpText);
}

CommandLineArg* TorcCommandLineParser::Add(QStringList Args, QString Name,
                                           int Default, QString HelpText,
                                           QString LongHelpText)
{
    return Add(Args, Name, QVariant::Int,
               QVariant(Default), HelpText, LongHelpText);
}

CommandLineArg* TorcCommandLineParser::Add(QStringList Args, QString Name,
                                           uint Default, QString HelpText,
                                           QString LongHelpText)
{
    return Add(Args, Name, QVariant::UInt,
               QVariant(Default), HelpText, LongHelpText);
}

CommandLineArg* TorcCommandLineParser::Add(QStringList Args, QString Name,
                                           long long Default, QString HelpText,
                                           QString LongHelpText)
{
    return Add(Args, Name, QVariant::LongLong,
               QVariant(Default), HelpText, LongHelpText);
}

CommandLineArg* TorcCommandLineParser::Add(QStringList Args, QString Name,
                                           double Default, QString HelpText,
                                           QString LongHelpText)
{
    return Add(Args, Name, QVariant::Double,
               QVariant(Default), HelpText, LongHelpText);
}

CommandLineArg* TorcCommandLineParser::Add(QStringList Args, QString Name,
                                           const char *Default, QString HelpText,
                                           QString LongHelpText)
{
    return Add(Args, Name, QVariant::String,
               QVariant(Default), HelpText, LongHelpText);
}

CommandLineArg* TorcCommandLineParser::Add(QStringList Args, QString Name,
                                           QString Default, QString HelpText,
                                           QString LongHelpText)
{
    return Add(Args, Name, QVariant::String,
               QVariant(Default), HelpText, LongHelpText);
}

CommandLineArg* TorcCommandLineParser::Add(QStringList Args, QString Name,
                                           QSize Default, QString HelpText,
                                           QString LongHelpText)
{
    return Add(Args, Name, QVariant::Size,
               QVariant(Default), HelpText, LongHelpText);
}

CommandLineArg* TorcCommandLineParser::Add(QStringList Args, QString Name,
                                           QDateTime Default, QString HelpText,
                                           QString LongHelpText)
{
    return Add(Args, Name, QVariant::DateTime,
               QVariant(Default), HelpText, LongHelpText);
}

CommandLineArg* TorcCommandLineParser::Add(QStringList Args, QString Name,
                                           QVariant::Type Type, QString HelpText,
                                           QString LongHelpText)
{
    return Add(Args, Name, Type,
               QVariant(Type), HelpText, LongHelpText);
}

/** \brief Replace dummy arguments used to define interdependency with pointers
 *  to their real counterparts.
 */
bool TorcCommandLineParser::ReconcileLinks(void)
{
    QList<CommandLineArg*> links;
    QMap<QString,CommandLineArg*>::iterator it;
    QList<CommandLineArg*>::iterator i2;

    if (m_verbose)
        cerr << "Reconciling links for option interdependencies." << endl;

    for (it = m_namedArgs.begin(); it != m_namedArgs.end(); ++it)
    {
        links = (*it)->m_parents;
        for (i2 = links.begin(); i2 != links.end(); ++i2)
        {
            if ((*i2)->m_type != QVariant::Invalid)
                continue; // already handled

            if (!m_namedArgs.contains((*i2)->m_name))
            {
                // not found
                cerr << "ERROR: could not reconcile linked argument." << endl
                     << "  '" << (*it)->m_name.toLocal8Bit().constData()
                     << "' could not find '"
                     << (*i2)->m_name.toLocal8Bit().constData() << "'." << endl
                     << "  Please resolve dependency and recompile." << endl;
                return false;
            }

            // replace linked argument
            if (m_verbose)
                cerr << QString("  Setting %1 as child of %2")
                            .arg((*it)->m_name).arg((*i2)->m_name)
                            .toLocal8Bit().constData()
                     << endl;
            (*it)->SetChildOf(m_namedArgs[(*i2)->m_name]);
        }

        links = (*it)->m_children;
        for (i2 = links.begin(); i2 != links.end(); ++i2)
        {
            if ((*i2)->m_type != QVariant::Invalid)
                continue; // already handled

            if (!m_namedArgs.contains((*i2)->m_name))
            {
                // not found
                cerr << "ERROR: could not reconcile linked argument." << endl
                     << "  '" << (*it)->m_name.toLocal8Bit().constData()
                     << "' could not find '"
                     << (*i2)->m_name.toLocal8Bit().constData() << "'." << endl
                     << "  Please resolve dependency and recompile." << endl;
                return false;
            }

            // replace linked argument
            if (m_verbose)
                cerr << QString("  Setting %1 as parent of %2")
                            .arg((*it)->m_name).arg((*i2)->m_name)
                            .toLocal8Bit().constData()
                     << endl;
            (*it)->SetParentOf(m_namedArgs[(*i2)->m_name]);
        }

        links = (*it)->m_requires;
        for (i2 = links.begin(); i2 != links.end(); ++i2)
        {
            if ((*i2)->m_type != QVariant::Invalid)
                continue; // already handled

            if (!m_namedArgs.contains((*i2)->m_name))
            {
                // not found
                cerr << "ERROR: could not reconcile linked argument." << endl
                     << "  '" << (*it)->m_name.toLocal8Bit().constData()
                     << "' could not find '"
                     << (*i2)->m_name.toLocal8Bit().constData() << "'." << endl
                     << "  Please resolve dependency and recompile." << endl;
                return false;
            }

            // replace linked argument
            if (m_verbose)
                cerr << QString("  Setting %1 as requiring %2")
                            .arg((*it)->m_name).arg((*i2)->m_name)
                            .toLocal8Bit().constData()
                     << endl;
            (*it)->SetRequires(m_namedArgs[(*i2)->m_name]);
        }

        i2 = (*it)->m_requiredby.begin();
        while (i2 != (*it)->m_requiredby.end())
        {
            if ((*i2)->m_type == QVariant::Invalid)
            {
                // if its not an invalid, it shouldnt be here anyway
                if (m_namedArgs.contains((*i2)->m_name))
                {
                    m_namedArgs[(*i2)->m_name]->SetRequires(*it);
                    if (m_verbose)
                        cerr << QString("  Setting %1 as blocking %2")
                                    .arg((*it)->m_name).arg((*i2)->m_name)
                                    .toLocal8Bit().constData()
                             << endl;
                }
            }

            (*i2)->DownRef();
            i2 = (*it)->m_requiredby.erase(i2);
        }

        i2 = (*it)->m_blocks.begin();
        while (i2 != (*it)->m_blocks.end())
        {
            if ((*i2)->m_type != QVariant::Invalid)
            {
                ++i2;
                continue; // already handled
            }

            if (!m_namedArgs.contains((*i2)->m_name))
            {
                (*i2)->DownRef();
                i2 = (*it)->m_blocks.erase(i2);
                continue; // if it doesnt exist, it cant block this command
            }

            // replace linked argument
            if (m_verbose)
                cerr << QString("  Setting %1 as blocking %2")
                            .arg((*it)->m_name).arg((*i2)->m_name)
                            .toLocal8Bit().constData()
                     << endl;
            (*it)->SetBlocks(m_namedArgs[(*i2)->m_name]);
            ++i2;
        }
    }

    return true;
}

/** \brief Returned stored QVariant for given argument, or default value
 *  if not used
 */
QVariant TorcCommandLineParser::operator[](const QString &Name)
{
    QVariant var("");
    if (!m_namedArgs.contains(Name))
        return var;

    CommandLineArg *arg = m_namedArgs[Name];

    if (arg->m_given)
        var = arg->m_stored;
    else
        var = arg->m_default;

    return var;
}

/** \brief Return list of additional values provided on the command line
 *  independent of any keyword.
 */
QStringList TorcCommandLineParser::GetArgs(void) const
{
    return ToStringList("_args");
}

/** \brief Return map of additional key/value pairs provided on the command
 *  line independent of any registered argument.
 */
QMap<QString,QString> TorcCommandLineParser::GetExtra(void) const
{
    return ToMap("_extra");
}

/** \brief Return any text supplied on the command line after a bare '--'
 */
QString TorcCommandLineParser::GetPassthrough(void) const
{
    return ToStringList("_passthrough").join(" ");
}

/** \brief Return map of key/value pairs provided to override database options
 *
 *  This method is used for the -O/--override-setting options, as well as the
 *  specific arguments to override the window border and mouse cursor. On its
 *  first use, this method will also read in any addition settings provided in
 *  the --override-settings-file
 */
QMap<QString,QString> TorcCommandLineParser::GetSettingsOverride(void)
{
    QMap<QString,QString> smap = ToMap("overridesettings");

    if (!m_overridesImported)
    {
        if (ToBool("overridesettingsfile"))
        {
            QString filename = ToString("overridesettingsfile");
            if (!filename.isEmpty())
            {
                QFile f(filename);
                if (f.open(QIODevice::ReadOnly))
                {
                    char buf[1024];
                    int64_t len = f.readLine(buf, sizeof(buf) - 1);
                    while (len != -1)
                    {
                        if (len >= 1 && buf[len-1]=='\n')
                            buf[len-1] = 0;
                        QString line(buf);
                        QStringList tokens = line.split("=",
                                QString::SkipEmptyParts);
                        if (tokens.size() == 2)
                        {
                            tokens[0].replace(QRegExp("^[\"']"), "");
                            tokens[0].replace(QRegExp("[\"']$"), "");
                            tokens[1].replace(QRegExp("^[\"']"), "");
                            tokens[1].replace(QRegExp("[\"']$"), "");
                            if (!tokens[0].isEmpty())
                                smap[tokens[0]] = tokens[1];
                        }
                        len = f.readLine(buf, sizeof(buf) - 1);
                    }
                }
                else
                {
                    QByteArray tmp = filename.toLatin1();
                    cerr << "Failed to open the override settings file: '"
                         << tmp.constData() << "'" << endl;
                }
            }
        }

        if (ToBool("windowed"))
            smap["RunFrontendInWindow"] = "1";
        else if (ToBool("notwindowed"))
            smap["RunFrontendInWindow"] = "0";

        if (ToBool("mousecursor"))
            smap["HideMouseCursor"] = "0";
        else if (ToBool("nomousecursor"))
            smap["HideMouseCursor"] = "1";

        m_overridesImported = true;

        if (!smap.isEmpty())
        {
            QVariantMap vmap;
            QMap<QString, QString>::const_iterator it;
            for (it = smap.begin(); it != smap.end(); ++it)
                vmap[it.key()] = QVariant(it.value());

            m_namedArgs["overridesettings"]->Set(QVariant(vmap));
        }
    }

    if (m_verbose)
    {
        cerr << "Option Overrides:" << endl;
        QMap<QString, QString>::const_iterator it;
        for (it = smap.constBegin(); it != smap.constEnd(); ++it)
            cerr << QString("    %1 - %2").arg(it.key(), 30).arg(*it)
                        .toLocal8Bit().constData() << endl;
    }

    return smap;
}

/** \brief Returns stored QVariant as a boolean
 *
 *  If the stored value is of type boolean, this will return the actual
 *  stored or default value. For all other types, this will return whether
 *  the argument was supplied on the command line or not.
 */
bool TorcCommandLineParser::ToBool(QString Key) const
{
    if (!m_namedArgs.contains(Key))
        return false;

    CommandLineArg *arg = m_namedArgs[Key];

    if (arg->m_type == QVariant::Bool)
    {
        if (arg->m_given)
            return arg->m_stored.toBool();
        return arg->m_default.toBool();
    }

    if (arg->m_given)
        return true;

    return false;
}

/** \brief Returns stored QVariant as an integer, falling to default
 *  if not provided
 */
int TorcCommandLineParser::ToInt(QString Key) const
{
    int val = 0;
    if (!m_namedArgs.contains(Key))
        return val;

    CommandLineArg *arg = m_namedArgs[Key];

    if (arg->m_given)
    {
        if (arg->m_stored.canConvert(QVariant::Int))
            val = arg->m_stored.toInt();
    }
    else
    {
        if (arg->m_default.canConvert(QVariant::Int))
            val = arg->m_default.toInt();
    }

    return val;
}

/** \brief Returns stored QVariant as an unsigned integer, falling to
 *  default if not provided
 */
uint TorcCommandLineParser::ToUInt(QString Key) const
{
    uint val = 0;
    if (!m_namedArgs.contains(Key))
        return val;

    CommandLineArg *arg = m_namedArgs[Key];

    if (arg->m_given)
    {
        if (arg->m_stored.canConvert(QVariant::UInt))
            val = arg->m_stored.toUInt();
    }
    else
    {
        if (arg->m_default.canConvert(QVariant::UInt))
            val = arg->m_default.toUInt();
    }

    return val;
}

/** \brief Returns stored QVariant as a long integer, falling to
 *  default if not provided
 */
long long TorcCommandLineParser::ToLongLong(QString Key) const
{
    long long val = 0;
    if (!m_namedArgs.contains(Key))
        return val;

    CommandLineArg *arg = m_namedArgs[Key];

    if (arg->m_given)
    {
        if (arg->m_stored.canConvert(QVariant::LongLong))
            val = arg->m_stored.toLongLong();
    }
    else
    {
        if (arg->m_default.canConvert(QVariant::LongLong))
            val = arg->m_default.toLongLong();
    }

    return val;
}

/** \brief Returns stored QVariant as double floating point value, falling
 *  to default if not provided
 */
double TorcCommandLineParser::ToDouble(QString Key) const
{
    double val = 0.0;
    if (!m_namedArgs.contains(Key))
        return val;

    CommandLineArg *arg = m_namedArgs[Key];

    if (arg->m_given)
    {
        if (arg->m_stored.canConvert(QVariant::Double))
            val = arg->m_stored.toDouble();
    }
    else
    {
        if (arg->m_default.canConvert(QVariant::Double))
            val = arg->m_default.toDouble();
    }

    return val;
}

/** \brief Returns stored QVariant as a QSize value, falling
 *  to default if not provided
 */
QSize TorcCommandLineParser::ToSize(QString Key) const
{
    QSize val(0,0);
    if (!m_namedArgs.contains(Key))
        return val;

    CommandLineArg *arg = m_namedArgs[Key];

    if (arg->m_given)
    {
        if (arg->m_stored.canConvert(QVariant::Size))
            val = arg->m_stored.toSize();
    }
    else
    {
        if (arg->m_default.canConvert(QVariant::Size))
            val = arg->m_default.toSize();
    }

    return val;
}

/** \brief Returns stored QVariant as a QString, falling
 *  to default if not provided
 */
QString TorcCommandLineParser::ToString(QString Key) const
{
    QString val("");
    if (!m_namedArgs.contains(Key))
        return val;

    CommandLineArg *arg = m_namedArgs[Key];

    if (arg->m_given)
    {
        if (arg->m_stored.canConvert(QVariant::String))
            val = arg->m_stored.toString();
    }
    else
    {
        if (arg->m_default.canConvert(QVariant::String))
            val = arg->m_default.toString();
    }

    return val;
}

/** \brief Returns stored QVariant as a QStringList, falling to default
 *  if not provided. Optional separator can be specified to split result
 *  if stored value is a QString.
 */
QStringList TorcCommandLineParser::ToStringList(QString Key, QString sep) const
{
    QVariant varval;
    QStringList val;
    if (!m_namedArgs.contains(Key))
        return val;

    CommandLineArg *arg = m_namedArgs[Key];

    if (arg->m_given)
    {
        varval = arg->m_stored;
    }
    else
        varval = arg->m_default;

    if (arg->m_type == QVariant::String && !sep.isEmpty())
        val = varval.toString().split(sep);
    else if (varval.canConvert(QVariant::StringList))
        val = varval.toStringList();

    return val;
}

/** \brief Returns stored QVariant as a QMap, falling
 *  to default if not provided
 */
QMap<QString,QString> TorcCommandLineParser::ToMap(QString Key) const
{
    QMap<QString, QString> val;
    QMap<QString, QVariant> tmp;
    if (!m_namedArgs.contains(Key))
        return val;

    CommandLineArg *arg = m_namedArgs[Key];

    if (arg->m_given)
    {
        if (arg->m_stored.canConvert(QVariant::Map))
            tmp = arg->m_stored.toMap();
    }
    else
    {
        if (arg->m_default.canConvert(QVariant::Map))
            tmp = arg->m_default.toMap();
    }

    QMap<QString, QVariant>::const_iterator i;
    for (i = tmp.begin(); i != tmp.end(); ++i)
        val[i.key()] = i.value().toString();

    return val;
}

/** \brief Returns stored QVariant as a QDateTime, falling
 *  to default if not provided
 */
QDateTime TorcCommandLineParser::ToDateTime(QString Key) const
{
    QDateTime val;
    if (!m_namedArgs.contains(Key))
        return val;

    CommandLineArg *arg = m_namedArgs[Key];

    if (arg->m_given)
    {
        if (arg->m_stored.canConvert(QVariant::DateTime))
            val = arg->m_stored.toDateTime();
    }
    else
    {
        if (arg->m_default.canConvert(QVariant::DateTime))
            val = arg->m_default.toDateTime();
    }

    return val;
}

/** \brief Specify that parser should allow and collect values provided
 *  independent of any keyword
 */
void TorcCommandLineParser::AllowArgs(bool Allow)
{
    if (m_namedArgs.contains("_args"))
    {
        if (!Allow)
            m_namedArgs.remove("_args");
    }
    else if (!Allow)
        return;

    CommandLineArg *arg = new CommandLineArg("_args", QVariant::StringList,
                                             QStringList());
    m_namedArgs["_args"] = arg;
}

/** \brief Specify that parser should allow and collect additional key/value
 *  pairs not explicitly defined for processing
 */
void TorcCommandLineParser::AllowExtras(bool Allow)
{
    if (m_namedArgs.contains("_extra"))
    {
        if (!Allow)
            m_namedArgs.remove("_extra");
    }
    else if (!Allow)
        return;

    QMap<QString,QVariant> vmap;
    CommandLineArg *arg = new CommandLineArg("_extra", QVariant::Map, vmap);

    m_namedArgs["_extra"] = arg;
}

/** \brief Specify that parser should allow a bare '--', and collect all
 *  subsequent text as a QString
 */
void TorcCommandLineParser::AllowPassthrough(bool Allow)
{
    if (m_namedArgs.contains("_passthrough"))
    {
        if (!Allow)
            m_namedArgs.remove("_passthrough");
    }
    else if (!Allow)
        return;

    CommandLineArg *arg = new CommandLineArg("_passthrough",
                                    QVariant::StringList, QStringList());
    m_namedArgs["_passthrough"] = arg;
}

/** \brief Canned argument definition for --help
 */
void TorcCommandLineParser::AddHelp(void)
{
    Add(QStringList( QStringList() << "-h" << "--help" << "--usage" ),
            "showhelp", "", "Display this help printout, or give detailed "
                            "information of selected option.",
            "Displays a list of all commands available for use with "
            "this application. If another option is provided as an "
            "argument, it will provide detailed information on that "
            "option.");
}

/** \brief Canned argument definition for --version
 */
void TorcCommandLineParser::AddVersion(void)
{
    Add("--version", "showversion", false, "Display version information.",
            "Display informtion about build, including:\n"
            " version, branch, protocol, library API, Qt "
            "and compiled options.");
}

/** \brief Canned argument definition for --windowed and -no-windowed
 */
void TorcCommandLineParser::AddWindowed(void)
{
    Add(QStringList( QStringList() << "-nw" << "--no-windowed" ),
            "notwindowed", false,
            "Prevent application from running in a window.", "")
        ->SetBlocks("windowed")
        ->SetGroup("User Interface");

    Add(QStringList( QStringList() << "-w" << "--windowed" ), "windowed",
            false, "Force application to run in a window.", "")
        ->SetGroup("User Interface");
}

/** \brief Canned argument definition for --mouse-cursor and --no-mouse-cursor
 */
void TorcCommandLineParser::AddMouse(void)
{
    Add("--mouse-cursor", "mousecursor", false,
            "Force visibility of the mouse cursor.", "")
        ->SetBlocks("nomousecursor")
        ->SetGroup("User Interface");

    Add("--no-mouse-cursor", "nomousecursor", false,
            "Force the mouse cursor to be hidden.", "")
        ->SetGroup("User Interface");
}

/** \brief Canned argument definition for --daemon
 */
void TorcCommandLineParser::AddDaemon(void)
{
    Add(QStringList( QStringList() << "-d" << "--daemon" ), "daemon", false,
            "Fork application into background after startup.",
            "Fork application into background, detatching from "
            "the local terminal.\nOften used with: "
            " --logpath --pidfile --user");
}

/** \brief Canned argument definition for --override-setting and
 *  --override-settings-file
 */
void TorcCommandLineParser::AddSettingsOverride(void)
{
    Add(QStringList( QStringList() << "-O" << "--override-setting" ),
            "overridesettings", QVariant::Map,
            "Override a single setting defined by a key=value pair.",
            "Override a single setting from the database using "
            "options defined as one or more key=value pairs\n"
            "Multiple can be defined by multiple uses of the "
            "-O option.");
    Add("--override-settings-file", "overridesettingsfile", "",
            "Define a file of key=value pairs to be "
            "loaded for setting overrides.", "");
}

/** \brief Canned argument definition for --chanid and --starttime
 */
void TorcCommandLineParser::AddRecording(void)
{
    Add("--chanid", "chanid", 0U,
            "Specify chanid of recording to operate on.", "")
        ->SetRequires("starttime");

    Add("--starttime", "starttime", QDateTime(),
            "Specify start time of recording to operate on.", "");
}

void TorcCommandLineParser::AddDisplay(void)
{
#ifdef USING_X11
    Add("-display", "display", "", "Specify X server to use.", "")
        ->SetGroup("User Interface");
#endif
}

void TorcCommandLineParser::AddUPnP(void)
{
    Add("--noupnp", "noupnp", false, "Disable use of UPnP.", "");
}

/** \brief Canned argument definition for all logging options, including
 *  --verbose, --quiet, --loglevel
 */
void TorcCommandLineParser::AddLogging(
    const QString &DefaultVerbosity, LogLevel DefaultLogging)
{
    DefaultLogging =
        ((DefaultLogging >= LOG_UNKNOWN) || (DefaultLogging <= LOG_ANY)) ?
        LOG_INFO : DefaultLogging;

    QString logLevelStr = GetLogLevelName(DefaultLogging);

    Add(QStringList( QStringList() << "-v" << "--verbose" ), "verbose",
        DefaultVerbosity,
        "Specify log filtering. Use '-v help' for level info.", "")
                ->SetGroup("Logging");
    Add(QStringList( QStringList() << "-q" << "--quiet"), "quiet", 0,
        "Don't log to the console (-q).  Don't log anywhere (-q -q)", "")
                ->SetGroup("Logging");
    Add("--loglevel", "loglevel", logLevelStr,
        QString(
            "Set the logging level.  All log messages at lower levels will be "
            "discarded.\n"
            "In descending order: emerg, alert, crit, err, warning, notice, "
            "info, debug\ndefaults to ") + logLevelStr, "")
                ->SetGroup("Logging");
}

void TorcCommandLineParser::AddPIDFile(void)
{
    Add(QStringList( QStringList() << "-p" << "--pidfile" ), "pidfile", "",
            "Write PID of application to filename.",
            "Write the PID of the currently running process as a single "
            "line to this file. Used for init scripts to know what "
            "process to terminate, and with log rotators "
            "to send a HUP signal to process to have it re-open files.");
}

void TorcCommandLineParser::AddJob(void)
{
    Add(QStringList( QStringList() << "-j" << "--jobid" ), "jobid", 0, "",
            "Intended for internal use only, specify the JobID to match "
            "up with in the database for additional information and the "
            "ability to update runtime status in the database.");
}

void TorcCommandLineParser::AddInFile(void)
{
    Add("--infile", "infile", "", "Input file URI", "");
}

void TorcCommandLineParser::AddOutFile(void)
{
    Add("--outfile", "outfile", "", "Output file URI", "");
}

LogLevel TorcCommandLineParser::GetLoggingLevel(void)
{
    QString setting = ToString("loglevel");
    if (setting.isEmpty())
        return LOG_INFO;

    LogLevel level = GetLogLevel(setting);
    if (level == LOG_UNKNOWN)
        cerr << "Unknown log level: " << setting.toLocal8Bit().constData() << endl;

    return level;
}

/** \brief Set a new stored value for an existing argument definition, or
 *  spawn a new definition store value in. Argument is subsequently marked
 *  as being provided on the command line.
 */
bool TorcCommandLineParser::SetValue(const QString &Key, QVariant Value)
{
    CommandLineArg *arg;

    if (!m_namedArgs.contains(Key))
    {
        QVariant val(Value);
        arg = new CommandLineArg(Key, val.type(), val);
        m_namedArgs.insert(Key, arg);
    }
    else
    {
        arg = m_namedArgs[Key];
        if (arg->m_type != Value.type())
            return false;
    }

    arg->Set(Value);
    return true;
}

bool OpenPidFile(ofstream &pidfs, const QString &pidfile)
{
    if (!pidfile.isEmpty())
    {
        pidfs.open(pidfile.toLatin1().constData());
        if (!pidfs)
        {
            cerr << "Could not open pid file: " << ENO_STR << endl;
            return false;
        }
    }
    return true;
}

/** \brief Drop permissions to the specified user
 */
bool SetUser(const QString &username)
{
    if (username.isEmpty())
        return true;

#ifdef _WIN32
    cerr << "--user option is not supported on Windows" << endl;
    return false;
#else // ! _WIN32
#if defined(__linux__) || defined(__LINUX__)
    // Check the current dumpability of core dumps, which will be disabled
    // by setuid, so we can re-enable, if appropriate
    int dumpability = prctl(PR_GET_DUMPABLE);
#endif
    struct passwd *user_info = getpwnam(username.toLocal8Bit().constData());
    const uid_t user_id = geteuid();

    if (user_id && (!user_info || user_id != user_info->pw_uid))
    {
        cerr << "You must be running as root to use the --user switch." << endl;
        return false;
    }
    else if (user_info && user_id == user_info->pw_uid)
    {
        LOG(VB_GENERAL, LOG_WARNING,
            QString("Already running as '%1'").arg(username));
    }
    else if (!user_id && user_info)
    {
        if (setenv("HOME", user_info->pw_dir,1) == -1)
        {
            cerr << "Error setting home directory." << endl;
            return false;
        }
        if (setgid(user_info->pw_gid) == -1)
        {
            cerr << "Error setting effective group." << endl;
            return false;
        }
        if (initgroups(user_info->pw_name, user_info->pw_gid) == -1)
        {
            cerr << "Error setting groups." << endl;
            return false;
        }
        if (setuid(user_info->pw_uid) == -1)
        {
            cerr << "Error setting effective user." << endl;
            return false;
        }
#if defined(__linux__) || defined(__LINUX__)
        if (dumpability && (prctl(PR_SET_DUMPABLE, dumpability) == -1))
            LOG(VB_GENERAL, LOG_WARNING, "Unable to re-enable core file "
                    "creation. Run without the --user argument to use "
                    "shell-specified limits.");
#endif
    }
    else
    {
        cerr << QString("Invalid user '%1' specified with --user")
                    .arg(username).toLocal8Bit().constData() << endl;
        return false;
    }
    return true;
#endif // ! _WIN32
}


/** \brief Fork application into background, and detatch from terminal
 */
int TorcCommandLineParser::Daemonize(void)
{

#ifdef _WIN32
    LOG(VB_GENERAL, LOG_WARNING, "Fixme: Daemonize not yet supported on windows.");
    return GENERIC_EXIT_DAEMONIZING_ERROR;
#else

    ofstream pidfs;
    if (!OpenPidFile(pidfs, ToString("pidfile")))
        return GENERIC_EXIT_PERMISSIONS_ERROR;

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        LOG(VB_GENERAL, LOG_WARNING, "Unable to ignore SIGPIPE");

    if (ToBool("daemon") && (daemon(0, 1) < 0))
    {
        cerr << "Failed to daemonize: " << ENO_STR << endl;
        return GENERIC_EXIT_DAEMONIZING_ERROR;
    }

    QString username = ToString("username");
    if (!username.isEmpty() && !SetUser(username))
        return GENERIC_EXIT_PERMISSIONS_ERROR;

    if (pidfs)
    {
        pidfs << getpid() << endl;
        pidfs.close();
    }

    return GENERIC_EXIT_OK;
#endif // !_WIN32
}
