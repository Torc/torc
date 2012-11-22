// Qt
#include <QGuiApplication>
#include <QScreen>

// Torc
#include "uidisplay.h"

UIDisplay::UIDisplay(QWidget *Widget)
  : UIDisplayBase(Widget)
{
}

UIDisplay::~UIDisplay()
{
}

bool UIDisplay::InitialiseDisplay(void)
{
    m_pixelSize    = GetGeometryPriv();
    m_physicalSize = GetPhysicalSizePriv();
    m_refreshRate  = GetRefreshRatePriv();
    m_screen       = GetScreenPriv();
    m_screenCount  = GetScreenCountPriv();

    Sanitise();

    return true;
}

double UIDisplay::GetRefreshRatePriv(void)
{
    QScreen* screen = QGuiApplication::primaryScreen();
    return screen->refreshRate();
}

QSize UIDisplay::GetPhysicalSizePriv(void)
{
    QScreen* screen = QGuiApplication::primaryScreen();
    QSizeF sizef = screen->physicalSize();
    return QSize(sizef.width(), sizef.height());
}
