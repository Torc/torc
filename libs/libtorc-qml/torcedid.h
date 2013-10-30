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
    TorcEDID                            ();
   ~TorcEDID();

    static TorcEDID   GetEDID           (QWindow* Window, int Screen);
    static QByteArray TrimEDID          (const QByteArray &EDID);
    qint16            PhysicalAddress   (void);
    int               GetAudioLatency   (bool Interlaced);
    int               GetVideoLatency   (bool Interlaced);
    QString           GetMSString       (void);

  private:
    void             Process            (bool Quiet = false);

  private:
    QByteArray       m_edidData;
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

class TORC_QML_PUBLIC TorcEDIDFactory
{
  public:
    TorcEDIDFactory();
    virtual ~TorcEDIDFactory();
    static TorcEDIDFactory* GetTorcEDIDFactory (void);
    TorcEDIDFactory*        NextFactory    (void) const;
    virtual void            GetEDID        (QMap<QPair<int,QString>,QByteArray > &EDIDMap,
                                            QWindow* Window, int Screen) = 0;

  protected:
    static TorcEDIDFactory* gTorcEDIDFactory;
    TorcEDIDFactory*        nextTorcEDIDFactory;
};

#endif // TORCEDID_H
