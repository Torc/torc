#ifndef UIACTIONS_H
#define UIACTIONS_H

// Qt
#include <QString>

class QKeyEvent;

class UIActions
{
  public:
    UIActions();
    virtual ~UIActions();

    int GetActionFromKey(QKeyEvent *Event);
};

#endif // UIACTIONS_H
