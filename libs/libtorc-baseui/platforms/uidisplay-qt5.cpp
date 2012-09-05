// Qt
#include <QGuiApplication>
#include <QScreen>

// Torc
#include "torclogging.h"
#include "uidisplay.h"

UIDisplay::UIDisplay(QWidget *Widget)
  : UIDisplayBase(Widget),
    m_physicalSize(400, 300),
    m_refreshRate(30.0)
{
}

UIDisplay::~UIDisplay()
{
}

bool UIDisplay::InitialiseDisplay(void)
{
    m_pixelSize    = GetGeometry();
    m_physicalSize = GetPhysicalSize();
    m_refreshRate  = GetRefreshRate();
    m_screen       = GetScreen();
    m_screenCount  = GetScreenCount();

    LOG(VB_GENERAL, LOG_INFO, QString("Using screen %1 of %2")
        .arg(m_screen + 1).arg(m_screenCount));
    LOG(VB_GENERAL, LOG_INFO, QString("Refresh rate: %1Hz").arg(m_refreshRate));
    LOG(VB_GENERAL, LOG_INFO, QString("Screen size : %1mm x %2mm")
        .arg(m_physicalSize.width()).arg(m_physicalSize.height()));
    LOG(VB_GENERAL, LOG_INFO, QString("Screen size : %1px x %2px")
        .arg(m_pixelSize.width()).arg(m_pixelSize.height()));

    return true;
}

qreal UIDisplay::GetRefreshRate(void)
{
    QScreen* screen = QGuiApplication::primaryScreen();
    return screen->refreshRate();
}

QSize UIDisplay::GetPhysicalSize(void)
{
    QScreen* screen = QGuiApplication::primaryScreen();
    QSizeF sizef = screen->physicalSize();
    return QSize(sizef.width(), sizef.height());
}
