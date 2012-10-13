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
    CommandLineArg::AllowOneOf(QList<CommandLineArg*>()
        << Add("--probe", "probe", false,
                "Probe the given URI for media content (audio, video and still images).", "")
                ->SetGroup("File")
                ->SetRequiredChild(QStringList("infile"))
        << Add("--play", "play", false,
                "Play the given URI", "")
                ->SetGroup("Media")
                ->SetRequiredChild(QStringList("infile")));

    AddHelp();
    AddVersion();
    AddLogging();
    AddPIDFile();
    AddSettingsOverride();
    AddInFile();
}

QString UtilsCommandLineParser::GetHelpHeader(void) const
{
    return "torc-utils provides a set of helper utilities for Torc.";
}
