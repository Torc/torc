#ifndef TORCEDID_H
#define TORCEDID_H


// Qt
#include <QMap>
#include <QPair>
#include <QWindow>
#include <QByteArray>

// Torc
#include "torcqmlexport.h"

class QMutex;

class TORC_QML_PUBLIC TorcEDID
{
  public:
    TorcEDID                            (const QByteArray &Data);
   ~TorcEDID();

    static void       RegisterEDID    (WId Window, int Screen);
    static QByteArray TrimEDID        (const QByteArray &EDID);
    static qint16     PhysicalAddress (void);
    static int        GetAudioLatency (bool Interlaced);
    static int        GetVideoLatency (bool Interlaced);
    QString           GetMSString     (void);

  protected:
    TorcEDID();

  protected:
    static TorcEDID* gTorcEDID;
    static QMutex*   gTorcEDIDLock;
    QByteArray       m_edidData;

  private:
    void             Process          (bool Quiet = false);

  private:
    qint16           m_physicalAddress;
    int              m_audioLatency;
    int              m_videoLatency;
    int              m_interlacedAudioLatency;
    int              m_interlacedVideoLatency;
    QString          m_productString;
    QString          m_name;
    QString          m_serialNumberStr;
    quint32          m_serialNumber;
};

class TORC_QML_PUBLIC EDIDFactory
{
  public:
    EDIDFactory();
    virtual            ~EDIDFactory    ();
    static EDIDFactory* GetEDIDFactory (void);
    EDIDFactory*        NextFactory    (void) const;
    virtual void        GetEDID        (QMap<QPair<int,QString>,QByteArray > &EDIDMap,
                                        WId Window, int Screen) = 0;

  protected:
    static EDIDFactory* gEDIDFactory;
    EDIDFactory*        nextEDIDFactory;
};

#endif // TORCEDID_H
