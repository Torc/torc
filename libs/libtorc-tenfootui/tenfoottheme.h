#ifndef TENFOOTTHEME_H
#define TENFOOTTHEME_H

// Torc
#include "torctenfootuiexport.h"
#include "uitheme.h"

class UIWindow;

class TORC_TENFOOTUI_PUBLIC TenfootTheme : public UITheme
{
    friend class TenfootThemeLoader;

  public:
    static int      kTenfootThemeType;
    static UITheme* Load           (bool Immediate, UIWindow *Owner, const QString &FileName = QString(""));
    static bool     ParseThemeFile (UITheme *Theme, UIWidget *Parent, QDomDocument *Document);

    virtual ~TenfootTheme();

  protected:
     TenfootTheme (QSize WindowSize);
};

#endif // TENFOOTTHEME_H
