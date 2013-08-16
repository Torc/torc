#ifndef TORCSERIALISER_H
#define TORCSERIALISER_H

// Qt
#include <QVariant>
#include <QStringList>
#include <QMap>
#include <QDateTime>

// Torc
#include "torchttprequest.h"

#define QVARIANT_ERROR QString("Error: QVariantList must contain only one variant type")

class TorcSerialiser
{
  public:
    TorcSerialiser();
    virtual ~TorcSerialiser();

    QByteArray*              Serialise      (const QVariant &Data, const QString &Type);
    virtual HTTPResponseType ResponseType   (void) = 0;

  protected:
    virtual void             Prepare        (void) = 0;
    virtual void             Begin          (void) = 0;
    virtual void             AddProperty    (const QString &Name, const QVariant &Value) = 0;
    virtual void             End            (void) = 0;

  protected:
    QByteArray *m_content;
};

#endif // TORCSERIALISER_H
