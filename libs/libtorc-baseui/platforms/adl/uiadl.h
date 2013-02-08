#ifndef UIADL_H
#define UIADL_H

//Qt
#include <QByteArray>

class UIADL
{
  public:
    static bool       ADLAvailable (void);
    static QByteArray GetADLEDID   (char *Display, int Screen, const QString Hint = QString());
};

#endif // UIADL_H
