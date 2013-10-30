#ifndef TORCQMLDISPLAY_H
#define TORCQMLDISPLAY_H

// Qt
#include <QObject>
#include <QWindow>
#include <QScreen>
#include <QGuiApplication>

// Torc
#include "torcedid.h"
#include "torcqmlexport.h"

class TORC_QML_PUBLIC TorcDisplayMode
{
  public:
    TorcDisplayMode(int Width, int Height, int Depth, double Rate, bool Interlaced, int Index);
    TorcDisplayMode();

    int    m_width;
    int    m_height;
    int    m_depth;
    double m_rate;
    bool   m_interlaced;
    int    m_index;
};

class TORC_QML_PUBLIC TorcQMLDisplay : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString screenName               READ getScreenName               NOTIFY screenNameChanged)
    Q_PROPERTY(int     screenNumber             READ getScreenNumber             NOTIFY screenNumberChanged)
    Q_PROPERTY(int     screenCount              READ getScreenCount              NOTIFY screenCountChanged)
    Q_PROPERTY(qreal   screenRefreshRate        READ getScreenRefreshRate        NOTIFY screenRefreshRateChanged)
    Q_PROPERTY(bool    screenInterlaced         READ getScreenInterlaced         NOTIFY screenInterlacedChanged)
    Q_PROPERTY(QSizeF  screenPhysicalSize       READ getScreenPhysicalSize       NOTIFY screenPhysicalSizeChanged)
    Q_PROPERTY(QSize   screenPixelSize          READ getScreenPixelSize          NOTIFY screenPixelSizeChanged)
    Q_PROPERTY(qreal   screenAspectRatio        READ getScreenAspectRatio        NOTIFY screenAspectRatioChanged)
    Q_PROPERTY(qreal   screenPixelAspectRatio   READ getScreenPixelAspectRatio   NOTIFY screenPixelAspectRatioChanged)
    Q_PROPERTY(qreal   screenCECPhysicalAddress READ getScreenCECPhysicalAddress NOTIFY screenCECPhysicalAddressChanged)

  public:
    static TorcQMLDisplay* Create               (QWindow *Window);

    TorcQMLDisplay(QWindow *Window);
    virtual ~TorcQMLDisplay();

  signals:
    void         screenNameChanged              (QString Name);
    void         screenNumberChanged            (int     Number);
    void         screenCountChanged             (int     Count);
    void         screenDepthChanged             (int     Depth);
    void         screenRefreshRateChanged       (qreal   Rate);
    void         screenInterlacedChanged        (bool    Interlaced);
    void         screenPhysicalSizeChanged      (QSizeF  Size);
    void         screenPixelSizeChanged         (QSize   Size);
    void         screenAspectRatioChanged       (qreal   Aspect);
    void         screenPixelAspectRatioChanged  (qreal   Aspect);
    void         screenCECPhysicalAddressChanged(int     Address);

  public slots:
    QString      getScreenName                  (void);
    int          getScreenNumber                (void);
    int          getScreenCount                 (void);
    int          getScreenDepth                 (void);
    qreal        getScreenRefreshRate           (void);
    bool         getScreenInterlaced            (void);
    QSizeF       getScreenPhysicalSize          (void);
    QSize        getScreenPixelSize             (void);
    qreal        getScreenAspectRatio           (void);
    qreal        getScreenPixelAspectRatio      (void);
    int          getScreenCECPhysicalAddress    (void);

  protected slots:
    void         setScreenName                  (const  QString &Name);
    void         setScreenNumber                (int    Number);
    void         setScreenCount                 (int    Count);
    void         setScreenDepth                 (int    Depth);
    void         setScreenRefreshRate           (qreal  Rate);
    void         setScreenInterlaced            (bool   Interlaced);
    void         setScreenPhysicalSize          (QSizeF Size);
    void         setScreenPixelSize             (QSize  Size);
    void         setScreenAspectRatio           (qreal  Aspect);
    void         setScreenPixelAspectRatio      (qreal  Aspect);
    void         setScreenCECPhysicalAddress    (int    Address);

    void         ScreenChanged                  (QScreen* Screen);
    void         ScreenDestroyed                (QObject* Screen);
    void         GeometryChanged                (const QRect &Geometry);
    void         PhysicalSizeChanged            (QSizeF   Size);
    void         PixelSizeChanged               (QSize    Size);
    void         AspectRatioChanged             (qreal    Aspect);

  protected:
    static qreal FixRefreshRate                 (qreal   Rate);
    static qreal FixAspectRatio                 (qreal   Aspect);
    static bool  IsStandardScreenRatio          (qreal   Aspect);
    virtual void UpdateScreenData               (void);

  protected:
    QString      screenName;
    int          screenNumber;
    int          screenCount;
    int          screenDepth;
    qreal        screenRefreshRate;
    bool         screenInterlaced;
    QSizeF       screenPhysicalSize;
    QSize        screenPixelSize;
    qreal        screenAspectRatio;
    qreal        screenPixelAspectRatio;
    int          screenCECPhysicalAddress;

    QWindow*     m_window;
    QScreen*     m_screen;
    QList<TorcDisplayMode> m_modes;
    TorcEDID     m_edid;

  private:
    Q_DISABLE_COPY(TorcQMLDisplay);
};

class TORC_QML_PUBLIC TorcQMLDisplayFactory
{
  public:
    TorcQMLDisplayFactory();
    virtual ~TorcQMLDisplayFactory();
    static TorcQMLDisplayFactory* GetTorcQMLDisplayFactory  (void);
    TorcQMLDisplayFactory*        NextTorcQMLDisplayFactory (void) const;
    virtual void                  ScoreTorcQMLDisplay       (QWindow *Window, int &Score) = 0;
    virtual TorcQMLDisplay*       GetTorcQMLDisplay         (QWindow *Window, int Score) = 0;

  protected:
    static TorcQMLDisplayFactory* gTorcQMLDisplayFactory;
    TorcQMLDisplayFactory*        nextTorcQMLDisplayFactory;
};


#endif // TORCQMLDISPLAY_H
