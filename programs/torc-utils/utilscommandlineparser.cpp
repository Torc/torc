// Torc
#include "utilscommandlineparser.h"

UtilsCommandLineParser::UtilsCommandLineParser()
  : TorcCommandLineParser()
{
    LoadArguments();
}

UtilsCommandLineParser::~UtilsCommandLineParser()
{
}

void UtilsCommandLineParser::LoadArguments(void)
{
    AddHelp();
    AddVersion();
    AddLogging();
    AddPIDFile();
    AddSettingsOverride();
}

QString UtilsCommandLineParser::GetHelpHeader(void) const
{
    return "Torc-utils provides a set of helper utilities for Torc.";
}
