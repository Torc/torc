#ifndef CLIENTCOMMANDLINEPARSER_H
#define CLIENTCOMMANDLINEPARSER_H

// Torc
#include "torccommandlineparser.h"

class ClientCommandLineParser : public TorcCommandLineParser
{
  public:
    ClientCommandLineParser();
    virtual ~ClientCommandLineParser();

    void LoadArguments(void);

  protected:
    QString GetHelpHeader(void) const;
};

#endif // CLIENTCOMMANDLINEPARSER_H
