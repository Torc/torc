#ifndef TORCHTTPSERVICETEST_H
#define TORCHTTPSERVICETEST_H

// Qt
#include <QObject>
#include <QDateTime>
#include <QStringList>
#include <QVariantMap>

// Torc
#include "torchttpservice.h"

class TorcHTTPServiceTest : public TorcHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",              "1.0.0")
    Q_CLASSINFO("EchoInt",              "methods=GET&HEAD")
    Q_CLASSINFO("EchoBool",             "methods=GET&HEAD")
    Q_CLASSINFO("EchoFloat",            "methods=GET&HEAD")
    Q_CLASSINFO("EchoUnsignedLongLong", "methods=GET&HEAD")
    Q_CLASSINFO("EchoStringList",       "methods=GET&HEAD,type=strings")
    Q_CLASSINFO("GetVariantMap",        "type=LevelOne")

  public:
    TorcHTTPServiceTest();

  public slots:
    void                GetVoid                (void);
    int                 EchoInt                (int  Value);
    bool                EchoBool               (bool Value);
    float               EchoFloat              (float Value);
    unsigned long long  EchoUnsignedLongLong   (unsigned long long Value);
    QStringList         EchoStringList         (const QString &Value1, const QString &Value2, const QString &Value3);

    QVariantMap         GetVariantMap          (void);
    QDateTime           GetTimeNow             (void);
    QDateTime           GetTimeNowUtc          (void);

    QVariantHash        GetUnsupportedHash     (void);
    void*               GetUnsupportedPointer  (void);

    void                GetUnsupportedParameter (QStringList Unsupported);
};

#endif // TORCHTTPSERVICETEST_H
