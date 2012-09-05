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

  protected:
    int   GetScreen      (void);
    int   GetScreenCount (void);
    QSize GetGeometry    (void);

  protected:
    QSize    m_pixelSize;
    int      m_screen;
    int      m_screenCount;
    QWidget *m_widget;
};

#endif // UIDISPLAYBASE_H
