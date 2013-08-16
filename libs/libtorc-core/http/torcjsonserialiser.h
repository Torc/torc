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
    bool             m_javaScriptType;
};

#endif // TORCJSONSERIALISER_H
