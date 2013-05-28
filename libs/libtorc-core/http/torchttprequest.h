#ifndef TORCHTTPREQUEST_H
#define TORCHTTPREQUEST_H

// Qt
#include <QMap>
#include <QPair>
#include <QString>

// Torc
#include "torccoreexport.h"

class TorcSerialiser;

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
    HTTPResponseDefault,
    HTTPResponseHTML,
    HTTPResponseXML,
    HTTPResponseJSON,
    HTTPResponseJSONJavascript,
    HTTPResponsePList,
    HTTPResponseBinaryPList,
    HTTPResponsePListApple,
    HTTPResponseBinaryPListApple,
    HTTPResponseFile
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
    HTTP_OK                  = 200,
    HTTP_BadRequest          = 400,
    HTTP_Unauthorized        = 401,
    HTTP_Forbidden           = 402,
    HTTP_NotFound            = 404,
    HTTP_MethodNotAllowed    = 405,
    HTTP_InternalServerError = 500
} HTTPStatus;

class TORC_CORE_PUBLIC TorcHTTPRequest
{
  public:
    static HTTPRequestType RequestTypeFromString    (const QString &Type);
    static HTTPProtocol    ProtocolFromString       (const QString &Protocol);
    static QString         ProtocolToString         (HTTPProtocol Protocol);
    static QString         StatusToString           (HTTPStatus   Status);
    static QString         ResponseTypeToString     (HTTPResponseType Response);
    static QString         AllowedToString          (int Allowed);
    static int             StringToAllowed          (const QString &Allowed);

  public:
    TorcHTTPRequest(const QString &Method, QMap<QString,QString> *Headers, QByteArray *Content);
    ~TorcHTTPRequest();

    bool                   KeepAlive                (void);
    void                   SetStatus                (HTTPStatus Status);
    void                   SetResponseType          (HTTPResponseType Type);
    void                   SetResponseContent       (QByteArray *Content);
    void                   SetAllowed               (int Allowed);
    HTTPType               GetHTTPType              (void);
    HTTPRequestType        GetHTTPRequestType       (void);
    QString                GetUrl                   (void);
    QString                GetPath                  (void);
    QString                GetMethod                (void);
    QMap<QString,QString>  Queries                  (void);
    QPair<QByteArray*,QByteArray*> Respond          (void);
    TorcSerialiser*        GetSerialiser            (void);

  protected:
    QString                m_fullUrl;
    QString                m_path;
    QString                m_method;
    QString                m_query;
    HTTPType               m_type;
    HTTPRequestType        m_requestType;
    HTTPProtocol           m_protocol;
    bool                   m_keepAlive;
    QMap<QString,QString> *m_headers;
    QMap<QString,QString>  m_queries;
    QByteArray            *m_content;

    int                    m_allowed;
    HTTPResponseType       m_responseType;
    HTTPStatus             m_responseStatus;
    QByteArray            *m_responseContent;
};

#endif // TORCHTTPREQUEST_H
