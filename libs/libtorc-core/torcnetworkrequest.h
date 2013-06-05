#ifndef TORCNETWORKREQUEST_H
#define TORCNETWORKREQUEST_H

// Qt
#include <QMetaType>

// Torc
#include "torcnetwork.h"

class TorcNetworkRequest : public TorcReferenceCounter
{
    friend class TorcNetwork;

  public:
    TorcNetworkRequest(const QNetworkRequest Request, QNetworkAccessManager::Operation Type, int BufferSize, int *Abort);

    bool            WaitForStart      (int Timeout);
    int             Peek              (char* Buffer, qint32 BufferSize, int Timeout);
    int             Read              (char* Buffer, qint32 BufferSize, int Timeout, bool Peek = false);
    qint64          Seek              (qint64 Offset);
    QByteArray      ReadAll           (int Timeout);
    int             BytesAvailable    (void);
    qint64          GetSize           (void);
    qint64          GetPosition       (void);
    void            SetReadSize       (int Size);
    void            SetRange          (int Start, int End = 0);
    void            DownloadProgress  (qint64 Received, qint64 Total);
    bool            CanByteServe      (void);
    QUrl            GetFinalURL       (void);
    QString         GetContentType    (void);

  protected:
    virtual ~TorcNetworkRequest();
    void            Write             (QNetworkReply *Reply);

  private:
    bool            WritePriv         (QNetworkReply *Reply, char* Buffer, int Size);

  protected:
    // internal state/type
    QNetworkAccessManager::Operation m_type;
    int            *m_abort;
    bool            m_started;
    qint64          m_positionInFile;
    qint64          m_rewindPositionInFile;
    int             m_readPosition;
    int             m_writePosition;
    QAtomicInt      m_availableToRead;
    int             m_bufferSize;
    int             m_reserveBufferSize;
    int             m_writeBufferSize;
    QByteArray      m_buffer;
    int             m_readSize;
    int             m_redirectionCount;
    TorcTimer      *m_readTimer;
    TorcTimer      *m_writeTimer;

    // QNetworkReply state
    bool            m_replyFinished;
    int             m_replyBytesAvailable;
    qint64          m_bytesReceived;
    qint64          m_bytesTotal;

    // request/reply details
    QNetworkRequest m_request;
    int             m_rangeStart;
    int             m_rangeEnd;
    int             m_httpStatus;
    qint64          m_contentLength;
    QString         m_contentType;
    bool            m_byteServingAvailable;
};

Q_DECLARE_METATYPE(TorcNetworkRequest*);
Q_DECLARE_METATYPE(QNetworkReply*);

#endif // TORCNETWORKREQUEST_H
