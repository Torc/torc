#ifndef TORCUSBPRIVWIN_H
#define TORCUSBPRIVWIN_H

// Qt
#include <QTimer>

// Torc
#include "torcusb.h"

class TorcUSBPrivWin : public QObject, public TorcUSBPriv
{
    Q_OBJECT

  public:
    TorcUSBPrivWin(TorcUSB *Parent);
    virtual ~TorcUSBPrivWin();

    void     Destroy             (void);
    void     Refresh             (void);

  public slots:
    void     Update              (void);

  private:
    QStringList m_devicePaths;
    QTimer     *m_timer;

};

#endif // TORCUSBPRIVWIN_H
