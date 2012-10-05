#ifndef UTILSCOMMANDLINEPARSER_H
#define UTILSCOMMANDLINEPARSER_H

// Torc
#include "torccommandlineparser.h"

class UtilsCommandLineParser : public TorcCommandLineParser
{
  public:
    UtilsCommandLineParser();
    virtual ~UtilsCommandLineParser();
    void    LoadArguments (void);

  protected:
    QString GetHelpHeader (void) const;
};

#endif // UTILSCOMMANDLINEPARSER_H
