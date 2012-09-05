#ifndef UIACTIONS_H
#define UIACTIONS_H

// Qt
#include <QString>

class QEvent;

class UIActions
{
  public:
    UIActions();
    virtual ~UIActions();

    int GetActionFromKey(QEvent *Event);
};

#endif // UIACTIONS_H
