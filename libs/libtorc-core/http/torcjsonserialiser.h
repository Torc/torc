#ifndef TORCJSONSERIALISER_H
#define TORCJSONSERIALISER_H

// Qt
#include <QTextStream>

// Torc
#include "torcserialiser.h"

class TorcJSONSerialiser : public TorcSerialiser
{
  public:
    TorcJSONSerialiser(bool Javascript = false);
    virtual ~TorcJSONSerialiser();

    HTTPResponseType ResponseType      (void);

  protected:
    void             Prepare            (void);
    void             Begin              (void);
    void             AddProperty        (const QString &Name, const QVariant &Value);
    void             End                (void);

  private:
    void             JSONfromVariant    (const QVariant &Value);
    void             JSONfromList       (const QVariantList &Value);
    void             JSONfromStringList (const QStringList &Value);
    void             JSONfromMap        (const QVariantMap &Value);
    void             JSONfromHash       (const QVariantHash &Value);
    QString          Encode             (const QString &String);

  private:
    bool             m_javaScriptType;
    QTextStream     *m_textStream;
    bool             m_needComma;
};

#endif // TORCJSONSERIALISER_H
