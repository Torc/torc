#ifndef TORCUSBPRIVWIN_H
#define TORCUSBPRIVWIN_H

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

  private:
    QStringList m_devicePaths;
};

#endif // TORCUSBPRIVWIN_H
