#include "clientcommandlineparser.h"

ClientCommandLineParser::ClientCommandLineParser()
  : TorcCommandLineParser()
{
    LoadArguments();
}

ClientCommandLineParser::~ClientCommandLineParser()
{
}

void ClientCommandLineParser::LoadArguments(void)
{
    AddHelp();
    AddVersion();
    AddLogging();
    AddPIDFile();
    AddSettingsOverride();
}

QString ClientCommandLineParser::GetHelpHeader(void) const
{
    return "Torc-client is the primary playback application for Torc. It "
           "is designed for use on a 'big' screen.";
}
