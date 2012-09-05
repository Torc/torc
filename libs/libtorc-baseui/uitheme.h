#ifndef UITHEME_H
#define UITHEME_H

// Qt
#include <QSize>
#include <QVector>
#include <QString>

// Torc
#include "torcbaseuiexport.h"
#include "uiwidget.h"

class QDomDocument;
class QDomElement;

class TORC_BASEUI_PUBLIC UITheme : public UIWidget
{
    friend class UIThemeLoader;
    friend class TenfootThemeLoader;

  public:
    typedef enum ThemeState
    {
        ThemeNull,
        ThemeError,
        ThemeLoadingInfo,
        ThemeInfoOnly,
        ThemeLoadingFull,
        ThemeReady
    } ThemeState;

    static int      kUIThemeType;
    static UITheme* Load           (const QString &Filename);
    static bool     ParseThemeInfo (UITheme *Theme, QDomDocument *Document);
    static QString  GetThemeFile   (const QString &ThemeType, const QString &Override = QString(""));

  public:
    virtual        ~UITheme();
    void            ActivateTheme  (void);
    void            SetState       (ThemeState State);
    ThemeState      GetState       (void);
    QString         GetDirectory   (void);

  protected:
    UITheme(QSize WindowSize = QSize(800,600));

    bool ValidateThemeInfo (void);

  protected:
    ThemeState m_state;
    QString    m_name;
    QString    m_description;
    QString    m_authorName;
    QString    m_authorEmail;
    QSizeF     m_size;
    qreal      m_aspectRatio;
    quint16    m_versionMajor;
    quint16    m_versionMinor;
    QString    m_rootWindow;
    QString    m_previewImage;
    QString    m_directory;
    QVector<QString> m_globals;

    QSizeF     m_windowSize;
};

#endif // UITHEME_H
