#ifndef UITHEMELOADER_H
#define UITHEMELOADER_H

// Qt
#include <QRunnable>

// Torc
#include "torcbaseuiexport.h"

class UITheme;

class TORC_BASEUI_PUBLIC UIThemeLoader : public QRunnable
{
  public:
    UIThemeLoader(const QString &FileName, UITheme *Theme);
    virtual ~UIThemeLoader();

  protected:
    bool         LoadThemeInfo(void);
    virtual void run();

  protected:
    QString      m_filename;
    UITheme     *m_theme;
};

#endif // UITHEMELOADER_H
