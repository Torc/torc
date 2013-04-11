#ifndef TENFOOTCLOCK_H
#define TENFOOTCLOCK_H

// Qt
#include <QDateTime>

// Torc
#include "uiwidget.h"

class UIText;

class TenfootClock : public UIWidget
{
  Q_OBJECT

  public:
    static  int  kTenfootClockType;
    TenfootClock(UIWidget *Root, UIWidget *Parent, const QString &Name, int Flags);
    virtual ~TenfootClock();

    Q_PROPERTY   (bool smooth READ GetSmooth WRITE SetSmooth)

    // UIWidget
    virtual bool Refresh        (quint64 TimeNow);
    virtual bool DrawSelf       (UIWindow* Window, qreal XOffset, qreal YOffset);
    virtual bool Finalise       (void);
    virtual UIWidget* CreateCopy (UIWidget *Parent, const QString &Newname = QString(""));
    virtual void CopyFrom       (UIWidget *Other);

  public slots:
    bool         GetSmooth      (void);
    void         SetSmooth      (bool Smooth);

  protected:
    virtual bool InitialisePriv (QDomElement *Element);

  protected:
    bool         m_smooth;
    quint64      m_nextUpdate;
    quint64      m_baseTickcount;
    QDateTime    m_baseTime;
    QDateTime    m_displayTime;

    UIWidget    *m_secondHand;
    UIWidget    *m_minuteHand;
    UIWidget    *m_hourHand;
    UIText      *m_text;

  protected:
    bool         smooth;

  private:
    Q_DISABLE_COPY(TenfootClock)
};

Q_DECLARE_METATYPE(TenfootClock*);
#endif // TENFOOTCLOCK_H
