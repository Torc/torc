#ifndef TORCWEBSOCKET_H
#define TORCWEBSOCKET_H

// Qt
#include <QTcpSocket>
#include <QObject>
#include <QUrl>

// Torc
#include "torccoreexport.h"
#include "torcqthread.h"

class TorcHTTPConnection;
class TorcHTTPRequest;
class TorcHTTPReader;
class TorcRPCRequest;

class TORC_CORE_PUBLIC TorcWebSocket : public QObject
{
    Q_OBJECT
    Q_ENUMS(WSVersion)
    Q_ENUMS(OpCode)
    Q_ENUMS(CloseCode)
    Q_FLAGS(WSSubProtocol)

  public:
    enum WSVersion
    {
        VersionUnknown = -1,
        Version0       = 0,
        Version4       = 4,
        Version5       = 5,
        Version6       = 6,
        Version7       = 7,
        Version8       = 8,
        Version13      = 13
    };

    enum WSSubProtocol
    {
        SubProtocolNone           = (0 << 0),
        SubProtocolJSONRPC        = (1 << 0)
    };

    Q_DECLARE_FLAGS(WSSubProtocols, WSSubProtocol);

    enum OpCode
    {
        OpContinuation = 0x0,
        OpText         = 0x1,
        OpBinary       = 0x2,
        OpReserved3    = 0x3,
        OpReserved4    = 0x4,
        OpReserved5    = 0x5,
        OpReserved6    = 0x6,
        OpReserved7    = 0x7,
        OpClose        = 0x8,
        OpPing         = 0x9,
        OpPong         = 0xA,
        OpReservedB    = 0xB,
        OpReservedC    = 0xC,
        OpReservedD    = 0xD,
        OpReservedE    = 0xE,
        OpReservedF    = 0xF
    };

    enum CloseCode
    {
        CloseNormal              = 1000,
        CloseGoingAway           = 1001,
        CloseProtocolError       = 1002,
        CloseUnsupportedDataType = 1003,
        CloseReserved1004        = 1004,
        CloseStatusCodeMissing   = 1005,
        CloseAbnormal            = 1006,
        CloseInconsistentData    = 1007,
        ClosePolicyViolation     = 1008,
        CloseMessageTooBig       = 1009,
        CloseMissingExtension    = 1010,
        CloseUnexpectedError     = 1011,
        CloseTLSHandshakeError   = 1015
    };

  public:
    TorcWebSocket(TorcQThread *Parent, TorcHTTPRequest *Request, QTcpSocket *Socket);
    TorcWebSocket(TorcQThread *Parent, const QString &Address, quint16 Port, WSSubProtocol Protocol = SubProtocolJSONRPC);
    ~TorcWebSocket();

    static bool     ProcessUpgradeRequest (TorcHTTPConnection *Connection, TorcHTTPRequest *Request, QTcpSocket *Socket);
    static QString  OpCodeToString        (OpCode Code);
    static QString  CloseCodeToString     (CloseCode Code);
    static QString  SubProtocolsToString  (WSSubProtocols Protocols);
    static WSSubProtocols       SubProtocolsFromString            (const QString &Protocols);
    static QList<WSSubProtocol> SubProtocolsFromPrioritisedString (const QString &Protocols);

  signals:
    void            ConnectionEstablished (void);
    void            NewRequest            (TorcRPCRequest *Request);
    void            RequestCancelled      (TorcRPCRequest *Request);

  public slots:
    void            Start                 (void);

  public:
    void            RemoteRequest         (TorcRPCRequest *Request);
    void            CancelRequest         (TorcRPCRequest *Request, int Wait = 1000 /*ms*/);
    void            WaitForNotifications  (void);

  protected slots:
    void            ReadyRead             (void);
    void            CloseSocket           (void);
    void            Connected             (void);
    void            Error                 (QAbstractSocket::SocketError);
    void            HandleRemoteRequest   (TorcRPCRequest *Request);
    void            HandleCancelRequest   (TorcRPCRequest *Request);

  protected:
    bool            event                 (QEvent *Event);

  private:
    OpCode          FormatForSubProtocol  (WSSubProtocol Protocol);
    void            SendFrame             (OpCode Code, QByteArray &Payload);
    void            HandlePing            (QByteArray &Payload);
    void            HandlePong            (QByteArray &Payload);
    void            HandleCloseRequest    (QByteArray &Close);
    void            InitiateClose         (CloseCode Close, const QString &Reason);
    void            ProcessPayload        (const QByteArray &Payload);

  private:
    enum ReadState
    {
        ReadHeader,
        Read16BitLength,
        Read64BitLength,
        ReadMask,
        ReadPayload
    };

    TorcQThread     *m_parent;
    bool             m_handShaking;
    TorcHTTPReader  *m_upgradeResponseReader;
    QString          m_challengeResponse;
    QString          m_address;
    quint16          m_port;
    TorcHTTPRequest *m_upgradeRequest;
    QTcpSocket      *m_socket;
    int              m_abort;
    bool             m_serverSide;
    ReadState        m_readState;
    bool             m_echoTest;

    WSSubProtocol    m_subProtocol;
    OpCode           m_subProtocolFrameFormat;
    bool             m_frameFinalFragment;
    OpCode           m_frameOpCode;
    bool             m_frameMasked;
    quint64          m_framePayloadLength;
    quint64          m_framePayloadReadPosition;
    QByteArray       m_frameMask;
    QByteArray       m_framePayload;

    QByteArray      *m_bufferedPayload;
    OpCode           m_bufferedPayloadOpCode;

    bool             m_closeReceived;
    bool             m_closeSent;

    int              m_currentRequestID;
    QMap<int,TorcRPCRequest*> m_currentRequests;
    QMap<int,int>    m_requestTimers;
    QAtomicInt       m_outstandingNotifications;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(TorcWebSocket::WSSubProtocols);

class TORC_CORE_PUBLIC TorcWebSocketThread : public TorcQThread
{
  public:
    TorcWebSocketThread(TorcHTTPRequest *Request, QTcpSocket *Socket);
    TorcWebSocketThread(const QString &Address, quint16 Port, TorcWebSocket::WSSubProtocol Protocol = TorcWebSocket::SubProtocolJSONRPC);
    ~TorcWebSocketThread();

    TorcWebSocket*      Socket   (void);

    void                Start    (void);
    void                Finish   (void);
    void                Shutdown (void);

  private:
    TorcWebSocket      *m_webSocket;
};

#endif // TORCWEBSOCKET_H
