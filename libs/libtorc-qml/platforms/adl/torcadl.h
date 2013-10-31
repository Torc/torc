#ifndef TORCADL_H
#define TORCADL_H

//Qt
#include <QByteArray>

class TorcADL
{
  public:
    static bool       ADLAvailable (void);
    static QByteArray GetADLEDID   (char *Display, int Screen, const QString Hint = QString());
};

#endif // TORCADL_H
