/* Class TorcQMLDisplay
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2013
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Torc
#include "torclogging.h"
#include "torcqmldisplay.h"

TorcDisplayMode::TorcDisplayMode()
  : m_width(0),
    m_height(0),
    m_depth(0),
    m_rate(0.0f),
    m_interlaced(false),
    m_index(-1)
{
}

TorcDisplayMode::TorcDisplayMode(int Width, int Height, int Depth, double Rate, bool Interlaced, int Index)
  : m_width(Width),
    m_height(Height),
    m_depth(Depth),
    m_rate(Rate),
    m_interlaced(Interlaced),
    m_index(Index)
{
}

qreal TorcQMLDisplay::FixRefreshRate(qreal Rate)
{
    static const qreal defaultrate = 60.0;
    if (Rate > 20.0 && Rate < 200.0)
        return Rate;
    return defaultrate;
}

qreal TorcQMLDisplay::FixAspectRatio(qreal Aspect)
{
    static qreal aspects[] = { 1.0f, 4.0f / 3.0f, 16.0f / 9.0f, 16.0f / 10.0f, 5.0f / 4.0f, 2.0f / 1.0f, 3.0f / 2.0f, 14.0f / 9.0f, 21.0f / 10.0f };
    qreal high = Aspect + 0.01f;
    qreal low =  Aspect - 0.01f;

    for (uint i = 0; i < sizeof(aspects) / sizeof(qreal); ++i)
        if (low < aspects[i] && high > aspects[i])
            return aspects[i];

    return Aspect;
}

bool TorcQMLDisplay::IsStandardScreenRatio(qreal Aspect)
{
    static qreal aspects[] = { 4.0f / 3.0f, 16.0f / 9.0f, 16.0f / 10.0f };
    qreal high = Aspect + 0.01f;
    qreal low =  Aspect - 0.01f;

    for (uint i = 0; i < sizeof(aspects) / sizeof(qreal); ++i)
        if (low < aspects[i] && high > aspects[i])
            return true;

    return false;
}

TorcQMLDisplay* TorcQMLDisplay::Create(QWindow *Window)
{
    TorcQMLDisplay* result = NULL;

    if (Window)
    {
        int score = 0;
        TorcQMLDisplayFactory* factory = TorcQMLDisplayFactory::GetTorcQMLDisplayFactory();
        for ( ; factory; factory = factory->NextTorcQMLDisplayFactory())
            (void)factory->ScoreTorcQMLDisplay(Window, score);

        factory = TorcQMLDisplayFactory::GetTorcQMLDisplayFactory();
        for ( ; factory; factory = factory->NextTorcQMLDisplayFactory())
        {
            result = factory->GetTorcQMLDisplay(Window, score);
            if (result)
            {
                result->ScreenChanged(Window->screen());
                break;
            }
        }
    }

    return result;
}

TorcQMLDisplay::TorcQMLDisplay(QWindow *Window)
  : QObject(NULL),
    screenName(tr("Unknown")),
    screenNumber(-1),
    screenCount(-1),
    screenDepth(-1),
    screenRefreshRate(-1.0f),
    screenInterlaced(false),
    screenPhysicalSize(QSize(0.0f, 0.0f)),
    screenPixelSize(0, 0),
    screenAspectRatio(-1.0f),
    screenPixelAspectRatio(-1.0f),
    screenCECPhysicalAddress(0x0000),
    m_window(Window),
    m_screen(NULL)
{
    // listen for screen changes
    connect(m_window, SIGNAL(screenChanged(QScreen*)), this, SLOT(ScreenChanged(QScreen*)));

    // screen aspect ratio is driven by its physical dimensions in mm
    connect(this, SIGNAL(screenPhysicalSizeChanged(QSizeF)), this, SLOT(PhysicalSizeChanged(QSizeF)));

    // pixel aspect ratio is driven by screen pixel size and screen aspect ratio
    connect(this, SIGNAL(screenPixelSizeChanged(QSize)),     this, SLOT(PixelSizeChanged(QSize)));
    connect(this, SIGNAL(screenAspectRatioChanged(qreal)),   this, SLOT(AspectRatioChanged(qreal)));
}

TorcQMLDisplay::~TorcQMLDisplay()
{
    disconnect();
}

QString TorcQMLDisplay::getScreenName(void)
{
    return screenName;
}

int TorcQMLDisplay::getScreenNumber(void)
{
    return screenNumber;
}

int TorcQMLDisplay::getScreenCount(void)
{
    return screenCount;
}

int TorcQMLDisplay::getScreenDepth(void)
{
    return screenDepth;
}

qreal TorcQMLDisplay::getScreenRefreshRate(void)
{
    return screenRefreshRate;
}

bool TorcQMLDisplay::getScreenInterlaced(void)
{
    return screenInterlaced;
}

QSizeF TorcQMLDisplay::getScreenPhysicalSize(void)
{
    return screenPhysicalSize;
}

QSize TorcQMLDisplay::getScreenPixelSize(void)
{
    return screenPixelSize;
}

qreal TorcQMLDisplay::getScreenAspectRatio(void)
{
    return screenAspectRatio;
}

qreal TorcQMLDisplay::getScreenPixelAspectRatio(void)
{
    return screenPixelAspectRatio;
}

int TorcQMLDisplay::getScreenCECPhysicalAddress(void)
{
    return screenCECPhysicalAddress;
}

void TorcQMLDisplay::setScreenName(const QString &Name)
{
    if (Name != screenName)
    {
        screenName = Name;
        LOG(VB_GENERAL, LOG_INFO, QString("Screen name: %1").arg(screenName));
        emit screenNameChanged(screenName);
    }
}

void TorcQMLDisplay::setScreenNumber(int Number)
{
    if (Number != screenNumber)
    {
        screenNumber = Number;
        LOG(VB_GENERAL, LOG_INFO, QString("Screen number: %1 of %2").arg(screenNumber + 1).arg(screenCount));
        emit screenNumberChanged(screenNumber);
    }
}

void TorcQMLDisplay::setScreenCount(int Count)
{
    if (Count != screenCount)
    {
        screenCount = Count;
        LOG(VB_GENERAL, LOG_INFO, QString("Screen count: %1").arg(screenCount));
        emit screenCountChanged(screenCount);
    }
}

void TorcQMLDisplay::setScreenDepth(int Depth)
{
    if (Depth != screenDepth)
    {
        screenDepth = Depth;
        LOG(VB_GENERAL, LOG_INFO, QString("Screen depth: %1").arg(screenDepth));
        emit screenDepthChanged(screenDepth);
    }
}

void TorcQMLDisplay::setScreenRefreshRate(qreal Rate)
{
    Rate = FixRefreshRate(Rate);

    if (!qFuzzyCompare(Rate + 1.0f, screenRefreshRate + 1.0f))
    {
        screenRefreshRate = Rate;
        LOG(VB_GENERAL, LOG_INFO, QString("Screen refresh rate: %1").arg(screenRefreshRate));
        emit screenRefreshRateChanged(screenRefreshRate);
    }
}

void TorcQMLDisplay::setScreenInterlaced(bool Interlaced)
{
    if (Interlaced != screenInterlaced)
    {
        screenInterlaced = Interlaced;
        LOG(VB_GENERAL, LOG_INFO, QString("Screen interlaced: %1").arg(screenInterlaced));
        emit screenInterlacedChanged(screenInterlaced);
    }
}

void TorcQMLDisplay::setScreenPhysicalSize(QSizeF Size)
{
    if (Size != screenPhysicalSize)
    {
        screenPhysicalSize = Size;
        LOG(VB_GENERAL, LOG_INFO, QString("Screen size: %1x%2 (mm)").arg(screenPhysicalSize.width(), 2, 'f', 2, '0')
                                                                    .arg(screenPhysicalSize.height(), 2, 'f', 2, '0'));
        emit screenPhysicalSizeChanged(screenPhysicalSize);
    }
}

void TorcQMLDisplay::setScreenPixelSize(QSize Size)
{
    if (Size != screenPixelSize)
    {
        screenPixelSize = Size;
        LOG(VB_GENERAL, LOG_INFO, QString("Screen size: %1x%2 (pixels)").arg(screenPixelSize.width()).arg(screenPixelSize.height()));
        emit screenPixelSizeChanged(screenPixelSize);
    }
}

void TorcQMLDisplay::setScreenAspectRatio(qreal Aspect)
{
    Aspect = FixAspectRatio(Aspect);

    if (!IsStandardScreenRatio(Aspect) && screenPixelSize.height() > 0)
    {
        // try to fix aspect ratios that are based on innacurate EDID physical size data (EDID only provides
        // centimeter accuracy by default). Use the raw pixel aspect ratio as a hint.
        qreal rawpar = (qreal)screenPixelSize.width() / (qreal)screenPixelSize.height();
        if (IsStandardScreenRatio(rawpar) && (abs(Aspect - rawpar) < 0.1))
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Adjusting screen aspect ratio from %1 to %2").arg(Aspect).arg(rawpar));
            Aspect = rawpar;
        }
    }

    if (!qFuzzyCompare(Aspect + 1.0f, screenAspectRatio + 1.0f))
    {
        screenAspectRatio = Aspect;
        LOG(VB_GENERAL, LOG_INFO, QString("Screen aspect ratio: %1").arg(screenAspectRatio));
        emit screenAspectRatioChanged(screenAspectRatio);
    }
}

void TorcQMLDisplay::setScreenPixelAspectRatio(qreal Aspect)
{
    Aspect = FixAspectRatio(Aspect);

    if (!qFuzzyCompare(Aspect + 1.0f, screenPixelAspectRatio + 1.0f))
    {
        screenPixelAspectRatio = Aspect;
        LOG(VB_GENERAL, LOG_INFO, QString("Screen pixel aspect ratio: %1").arg(screenPixelAspectRatio));
        emit screenPixelAspectRatioChanged(screenPixelAspectRatio);
    }
}

void TorcQMLDisplay::setScreenCECPhysicalAddress(int Address)
{
    if (Address != screenCECPhysicalAddress)
    {
        screenCECPhysicalAddress = Address;
        LOG(VB_GENERAL, LOG_INFO, QString("Screen physical address: 0x%1").arg(screenCECPhysicalAddress, 4, 16, QChar('0')));
        emit screenCECPhysicalAddressChanged(screenCECPhysicalAddress);
    }
}

/*! \brief Notification that the current QScreen has been destroyed.
 *  \note Never actually seems to be triggered.
*/
void TorcQMLDisplay::ScreenDestroyed(QObject *Screen)
{
    QScreen* screen = static_cast<QScreen*>(Screen);
    if (screen && m_screen && (m_screen == screen))
    {
        LOG(VB_GENERAL, LOG_INFO, "Current screen deleted");
        m_screen = NULL;
    }
}

void TorcQMLDisplay::ScreenChanged(QScreen* Screen)
{
    if (!m_window)
        return;

    if (m_screen && (m_screen == Screen))
        return;

    LOG(VB_GENERAL, LOG_INFO, "New screen");

    // disconnect from existing screen
    if (m_screen)
        disconnect(m_screen, 0, this, 0);

    m_screen = Screen;
    if (!m_screen)
        return;

    // listen for screen deletion
    connect(m_screen, SIGNAL(destroyed(QObject*)), this, SLOT(ScreenDestroyed(QObject*)));

    // listen for changes in the current screen
    connect(m_screen, SIGNAL(refreshRateChanged(qreal)),   this, SLOT(setScreenRefreshRate(qreal)));
    connect(m_screen, SIGNAL(physicalSizeChanged(QSizeF)), this, SLOT(setScreenPhysicalSize(QSizeF)));
    connect(m_screen, SIGNAL(geometryChanged(QRect)),      this, SLOT(GeometryChanged(QRect)));

    UpdateScreenData();
}

void TorcQMLDisplay::GeometryChanged(const QRect &Geometry)
{
    setScreenPixelSize(Geometry.size());
}

void TorcQMLDisplay::PhysicalSizeChanged(QSizeF Size)
{
    (void)Size;
    if (screenPhysicalSize.height() > 0)
        setScreenAspectRatio(screenPhysicalSize.width() / screenPhysicalSize.height());
}

void TorcQMLDisplay::PixelSizeChanged(QSize Size)
{
    (void)Size;
    if (screenPixelSize.height() > 0 && screenAspectRatio > 0.0f)
        setScreenPixelAspectRatio(((qreal)screenPixelSize.width() / (qreal)screenPixelSize.height()) / screenAspectRatio);
}

void TorcQMLDisplay::AspectRatioChanged(qreal Aspect)
{
    (void)Aspect;
    if (screenPixelSize.height() > 0 && screenAspectRatio > 0.0f)
        setScreenPixelAspectRatio(((qreal)screenPixelSize.width() / (qreal)screenPixelSize.height()) / screenAspectRatio);
}

void TorcQMLDisplay::UpdateScreenData(void)
{
    if (!m_window || !m_screen)
        return;

    // set current properties
    int screennumber = 0;
    QList<QScreen*> screens = QGuiApplication::screens();
    for ( ; screennumber < screens.size(); screennumber++)
        if (screens.at(screennumber) == m_screen)
            break;

    setScreenName(m_screen->name());
    // set screenCount first so logs are a little more sane
    setScreenCount(screens.size());
    setScreenNumber(screennumber);
    setScreenDepth(m_screen->depth());
    setScreenRefreshRate(m_screen->refreshRate());
    setScreenInterlaced(false);
    // set the pixel size first to aid with rounding adjustments in setScreenAspectRatio
    setScreenPixelSize(m_screen->size());
    setScreenPhysicalSize(m_screen->physicalSize());
}

class TorcQMLDisplayFactoryDefault : public TorcQMLDisplayFactory
{
    void ScoreTorcQMLDisplay(QWindow *Window, int &Score)
    {
        if (Score < 10)
            Score = 10;
    }

    TorcQMLDisplay* GetTorcQMLDisplay(QWindow *Window, int Score)
    {
        if (Score == 10)
            return new TorcQMLDisplay(Window);
        return NULL;
    }
} TorcQMLDisplayFactoryDefault;

TorcQMLDisplayFactory* TorcQMLDisplayFactory::gTorcQMLDisplayFactory = NULL;

TorcQMLDisplayFactory::TorcQMLDisplayFactory()
{
    nextTorcQMLDisplayFactory = gTorcQMLDisplayFactory;
    gTorcQMLDisplayFactory = this;
}

TorcQMLDisplayFactory::~TorcQMLDisplayFactory()
{
}

TorcQMLDisplayFactory* TorcQMLDisplayFactory::GetTorcQMLDisplayFactory(void)
{
    return gTorcQMLDisplayFactory;
}

TorcQMLDisplayFactory* TorcQMLDisplayFactory::NextTorcQMLDisplayFactory(void) const
{
    return nextTorcQMLDisplayFactory;
}
