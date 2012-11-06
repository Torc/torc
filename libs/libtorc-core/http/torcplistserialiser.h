#ifndef TORCPLISTSERIALISER_H
#define TORCPLISTSERIALISER_H

// Torc
#include "torcxmlserialiser.h"

class TorcPListSerialiser : public TorcXMLSerialiser
{
  public:
    TorcPListSerialiser();
    virtual ~TorcPListSerialiser();

    HTTPResponseType ResponseType        (void);

  protected:
    void             Begin               (void);
    void             AddProperty         (const QString &Name, const QVariant &Value);
    void             End                 (void);

    void             PListFromVariant    (const QString &Name, const QVariant &Value, bool NeedKey = true);
    void             PListFromList       (const QString &Name, const QVariantList &Value);
    void             PListFromStringList (const QString &Name, const QStringList &Value);
    void             PListFromMap        (const QString &Name, const QVariantMap &Value);
    void             PListFromHash       (const QString &Name, const QVariantHash &Value);
};

#endif // TORCPLISTSERIALISER_H
