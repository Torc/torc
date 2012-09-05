#ifndef TENFOOTTHEMELOADER_H
#define TENFOOTTHEMELOADER_H

// Torc
#include "uiwindow.h"
#include "uithemeloader.h"

class QThread;

class TenfootThemeLoader : public UIThemeLoader
{
  public:
    TenfootThemeLoader(const QString &FileName, UIWindow *Owner, QThread *OwnerThread);
    virtual ~TenfootThemeLoader();
    UITheme*     LoadTenfootTheme(void);

  protected:
    virtual void run();

  private:
    UIWindow    *m_owner;
    QThread     *m_ownerThread;
};

#endif // TENFOOTTHEMELOADER_H
