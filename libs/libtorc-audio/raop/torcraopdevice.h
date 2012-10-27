#ifndef TORCRAOPDEVICE_H
#define TORCRAOPDEVICE_H

// Qt
#include <QTcpServer>

class QMutex;
class TorcRAOPConnection;

class TorcRAOPDevice : public QTcpServer
{
    Q_OBJECT

  public:
    static     QMutex*            gTorcRAOPLock;
    static     TorcRAOPDevice*    gTorcRAOPDevice;

    static     void Enable        (bool Enable);
    static     QByteArray* Read   (int  Reference);

  public:
    virtual    ~TorcRAOPDevice    ();

  signals:
    
  public slots:
    void        NewConnection     (void);
    void        DeleteConnection  (void);

  protected:
    explicit    TorcRAOPDevice    ();
    bool        Open              (void);
    void        Close             (bool Suspend = false);
    bool        event             (QEvent *Event);
    QByteArray* ReadPacket        (int Reference);

  private:
    int                            m_port;
    QString                        m_macAddress;
    quint32                        m_bonjourReference;
    QMutex                        *m_lock;
    QMap<int, TorcRAOPConnection*> m_connections;
};

#endif // TORCRAOPDEVICE_H
