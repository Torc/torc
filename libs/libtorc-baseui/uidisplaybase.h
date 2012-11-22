#ifndef UIDISPLAYBASE_H
#define UIDISPLAYBASE_H

// Qt
#include <QWidget>
#include <QSize>

class UIDisplayBase
{
  public:
    UIDisplayBase(QWidget *Widget);
    virtual ~UIDisplayBase();

    int      GetScreen              (void);
    int      GetScreenCount         (void);
    QSize    GetGeometry            (void);
    double   GetRefreshRate         (void);
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
    double   m_aspectRatio;
    double   m_pixelAspectRatio;
    QWidget *m_widget;
};

#endif // UIDISPLAYBASE_H
