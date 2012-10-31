#ifndef TORCHTTPREQUEST_H
#define TORCHTTPREQUEST_H

// Qt
#include <QMap>
#include <QPair>
#include <QString>

// Torc
#include "torccoreexport.h"

typedef enum
{
    HTTPRequest,
    HTTPResponse
} HTTPType;

typedef enum
{
    HTTPUnknownType = 0,
    HTTPHead,
    HTTPGet,
    HTTPPost
} HTTPRequestType;

typedef enum
{
    HTTPResponseUnknown = 0,
    HTTPResponseDefault,
    HTTPResponseHTML,
    HTTPResponseXML,
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

  public:
    TorcHTTPRequest(const QString &Method, QMap<QString,QString> *Headers, QByteArray *Content);
    ~TorcHTTPRequest();

    bool                   KeepAlive                (void);
    void                   SetStatus                (HTTPStatus Status);
    void                   SetResponseType          (HTTPResponseType Type);
    void                   SetResponseContent       (QByteArray *Content);
    HTTPType               GetHTTPType              (void);
    QString                GetPath                  (void);
    QString                GetMethod                (void);
    QMap<QString,QString>  Queries                  (void);
    QPair<QByteArray*,QByteArray*> Respond          (void);

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

    HTTPResponseType       m_responseType;
    HTTPStatus             m_responseStatus;
    QByteArray            *m_responseContent;
};

#endif // TORCHTTPREQUEST_H
