#ifndef TORCQMLUTILS_H
#define TORCQMLUTILS_H

// Qt
#include <QtCore/QString>
#include <QtCore/QObject>
#include <QtQml/QQmlContext>

// Torc
#include "torcqmlexport.h"

class TORC_QML_PUBLIC TorcQMLUtils
{
  public:
    static void AddProperty(const QString &Name, QObject* Property, QQmlContext *Context);
};

#endif // TORCQMLUTILS_H
