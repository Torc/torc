// Torc
#include "audioclientcommandlineparser.h"

AudioClientCommandLineParser::AudioClientCommandLineParser()
  : TorcCommandLineParser()
{
    LoadArguments();
}

AudioClientCommandLineParser::~AudioClientCommandLineParser()
{
}

void AudioClientCommandLineParser::LoadArguments(void)
{
    AddHelp();
    AddVersion();
    AddLogging();
    AddPIDFile();
    AddSettingsOverride();
    AddInFile();
}

QString AudioClientCommandLineParser::GetHelpHeader(void) const
{
    return "torc-audioclient is the audio only media client for Torc.";
}
