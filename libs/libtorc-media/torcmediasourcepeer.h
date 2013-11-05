#ifndef TORCMEDIASOURCEPEER_H
#define TORCMEDIASOURCEPEER_H

// Qt
#include <QMap>
#include <QObject>

class TorcMediaPeer;
class TorcRPCRequest;

class TorcMediaSourcePeer : public QObject
{
    Q_OBJECT

    friend class TorcMediaSourcePeerObject;

  protected:
    TorcMediaSourcePeer();
   ~TorcMediaSourcePeer();

  public slots:
    void            PeerConnected       (QString Name, QString UUID);
    void            PeerDisconnected    (QString Name, QString UUID);
    void            RequestReady        (TorcRPCRequest *Request);
    void            ServiceNotification (QString Method);

  private:
    QMap<QString,TorcMediaPeer*> m_peers;
};

#endif // TORCMEDIASOURCEPEER_H
