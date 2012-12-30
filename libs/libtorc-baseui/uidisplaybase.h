#ifndef UIDISPLAYBASE_H
#define UIDISPLAYBASE_H

// Qt
#include <QWidget>
#include <QSize>

// Torc
#include "torcbaseuiexport.h"

class UIDisplayMode
{
  public:
    UIDisplayMode(int Width, int Height, int Depth, double Rate, bool Interlaced, int Index);
    UIDisplayMode();

    int    m_width;
    int    m_height;
    int    m_depth;
    double m_rate;
    bool   m_interlaced;
    int    m_index;
};

class TORC_BASEUI_PUBLIC UIDisplayBase
{
  public:
    UIDisplayBase(QWidget *Widget);
    virtual ~UIDisplayBase();

    bool     CanHandleVideoRate     (double Rate, int &ModeIndex);

    int      GetScreen              (void);
    int      GetScreenCount         (void);
    QSize    GetGeometry            (void);
    double   GetRefreshRate         (void);
    double   GetDefaultRefreshRate  (void);
    int      GetDefaultMode         (void);
    QSize    GetPhysicalSize        (void);
    double   GetDisplayAspectRatio  (void);
    double   GetPixelAspectRatio    (void);

  protected:
    int      GetScreenPriv          (void);
    int      GetScreenCountPriv     (void);
    QSize    GetGeometryPriv        (void);
    void     Sanitise               (void);

  protected:
    QSize    m_pixelSize;
    int      m_screen;
    int      m_screenCount;
    QSize    m_physicalSize;
    double   m_refreshRate;
    double   m_originalRefreshRate;
    int      m_originalModeIndex;
    bool     m_variableRefreshRate;
    double   m_aspectRatio;
    double   m_pixelAspectRatio;
    QWidget *m_widget;

    double   m_lastRateChecked;
    QList<UIDisplayMode> m_modes;
};

#endif // UIDISPLAYBASE_H
