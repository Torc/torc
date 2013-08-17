#ifndef TORCXMLSERIALISER_H
#define TORCXMLSERIALISER_H

// Qt
#include <QXmlStreamWriter>

// Torc
#include "torcserialiser.h"

class TorcXMLSerialiser : public TorcSerialiser
{
  public:
    TorcXMLSerialiser();
    virtual ~TorcXMLSerialiser();

    virtual HTTPResponseType ResponseType   (void);

  protected:
    virtual void             Prepare         (void);
    virtual void             Begin           (void);
    virtual void             AddProperty     (const QString &Name, const QVariant &Value);
    virtual void             End             (void);

    void                     VariantToXML    (const QString &Name, const QVariant &Value);
    void                     ListToXML       (const QString &Name, const QVariantList &Value);
    void                     StringListToXML (const QString &Name, const QStringList &Value);
    void                     MapToXML        (const QString &Name, const QVariantMap &Value);
    QString                  ContentName     (const QString &Name);

  protected:
    QXmlStreamWriter *m_xmlStream;
};

#endif // TORCXMLSERIALISER_H
