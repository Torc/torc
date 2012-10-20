#ifndef UTILSCOMMANDLINEPARSER_H
#define UTILSCOMMANDLINEPARSER_H

// Torc
#include "torccommandlineparser.h"

class AudioClientCommandLineParser : public TorcCommandLineParser
{
  public:
    AudioClientCommandLineParser();
    virtual ~AudioClientCommandLineParser();
    void    LoadArguments (void);

  protected:
    QString GetHelpHeader (void) const;
};

#endif // UTILSCOMMANDLINEPARSER_H
