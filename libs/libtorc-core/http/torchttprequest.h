#ifndef TORCHTTPREQUEST_H
#define TORCHTTPREQUEST_H

// Qt
#include <QMap>
#include <QPair>
#include <QString>

// Torc
#include "torccoreexport.h"

class TorcHTTPReader;
class TorcSerialiser;
class QTcpSocket;
class QFile;

typedef enum
{
    HTTPRequest,
    HTTPResponse
} HTTPType;

typedef enum
{
    HTTPUnknownType = (0 << 0),
    HTTPHead        = (1 << 0),
    HTTPGet         = (1 << 1),
    HTTPPost        = (1 << 2),
    HTTPPut         = (1 << 3),
    HTTPDelete      = (1 << 4),
    HTTPOptions     = (1 << 5)
} HTTPRequestType;

typedef enum
{
    HTTPResponseUnknown = 0,
    HTTPResponseNone,
    HTTPResponseDefault,
    HTTPResponseHTML,
    HTTPResponseXML,
    HTTPResponseJSON,
    HTTPResponseJSONJavascript,
    HTTPResponsePList,
    HTTPResponseBinaryPList,
    HTTPResponsePListApple,
    HTTPResponseBinaryPListApple
} HTTPResponseType;

typedef enum
{
    HTTPUnknownProtocol = 0,
    HTTPZeroDotNine,
    HTTPOneDotZero,
    HTTPOneDotOne
} HTTPProtocol;

typedef enum
{
    HTTP_SwitchingProtocols  = 101,
    HTTP_OK                  = 200,
    HTTP_PartialContent      = 206,
    HTTP_MovedPermanently    = 301,
    HTTP_BadRequest          = 400,
    HTTP_Unauthorized        = 401,
    HTTP_Forbidden           = 402,
    HTTP_NotFound            = 404,
    HTTP_MethodNotAllowed    = 405,
    HTTP_RequestedRangeNotSatisfiable = 416,
    HTTP_InternalServerError = 500
} HTTPStatus;

typedef enum
{
    HTTPConnectionClose     = 0,
    HTTPConnectionKeepAlive = 1,
    HTTPConnectionUpgrade   = 2
} HTTPConnection;

#define READ_CHUNK_SIZE (1024 *64)

class TORC_CORE_PUBLIC TorcHTTPRequest
{
  public:
    static HTTPRequestType RequestTypeFromString    (const QString &Type);
    static HTTPProtocol    ProtocolFromString       (const QString &Protocol);
    static HTTPStatus      StatusFromString         (const QString &Status);
    static QString         ProtocolToString         (HTTPProtocol Protocol);
    static QString         StatusToString           (HTTPStatus   Status);
    static QString         ResponseTypeToString     (HTTPResponseType Response);
    static QString         AllowedToString          (int Allowed);
    static QString         ConnectionToString       (HTTPConnection Connection);
    static int             StringToAllowed          (const QString &Allowed);
    static QList<QPair<quint64,quint64> > StringToRanges (const QString &Ranges, qint64 Size, qint64& SizeToSend);
    static QString         RangeToString            (const QPair<quint64,quint64> &Range, qint64 Size);

  public:
    TorcHTTPRequest(TorcHTTPReader *Reader);
    TorcHTTPRequest(const QString &Method, QMap<QString,QString> *Headers, QByteArray *Content);
    ~TorcHTTPRequest();

    void                   SetConnection            (HTTPConnection Connection);
    void                   SetStatus                (HTTPStatus Status);
    void                   SetResponseType          (HTTPResponseType Type);
    void                   SetResponseContent       (QByteArray *Content);
    void                   SetResponseFile          (QFile *File);
    void                   SetResponseHeader        (const QString &Header, const QString &Value);
    void                   SetAllowed               (int Allowed);
    HTTPStatus             GetHTTPStatus            (void);
    HTTPType               GetHTTPType              (void);
    HTTPRequestType        GetHTTPRequestType       (void);
    HTTPProtocol           GetHTTPProtocol          (void);
    QString                GetUrl                   (void);
    QString                GetPath                  (void);
    QString                GetMethod                (void);
    QMap<QString,QString>* Headers                  (void);
    QMap<QString,QString>  Queries                  (void);
    void                   Respond                  (QTcpSocket *Socket, int* Abort);
    void                   Redirected               (const QString &Redirected);
    TorcSerialiser*        GetSerialiser            (void);

  protected:
    void                   Initialise               (const QString &Method);

  protected:
    QString                m_fullUrl;
    QString                m_path;
    QString                m_method;
    QString                m_query;
    QString                m_redirectedTo;
    HTTPType               m_type;
    HTTPRequestType        m_requestType;
    HTTPProtocol           m_protocol;
    HTTPConnection         m_connection;
    QList<QPair<quint64,quint64> > m_ranges;
    QMap<QString,QString> *m_headers;
    QMap<QString,QString>  m_queries;
    QByteArray            *m_content;

    int                    m_allowed;
    HTTPResponseType       m_responseType;
    HTTPStatus             m_responseStatus;
    QByteArray            *m_responseContent;
    QFile                 *m_responseFile;
    QMap<QString,QString> *m_responseHeaders;
};

#endif // TORCHTTPREQUEST_H
