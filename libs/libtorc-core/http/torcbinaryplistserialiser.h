#ifndef TORCBINARYPLISTSERIALISER_H
#define TORCBINARYPLISTSERIALISER_H

// Qt
#include <QDataStream>

// Torc
#include "torcserialiser.h"

class TorcBinaryPListSerialiser : public TorcSerialiser
{
  public:
    TorcBinaryPListSerialiser();
    virtual ~TorcBinaryPListSerialiser();

    HTTPResponseType ResponseType         (void);

  protected:
    void             Prepare              (void);
    void             Begin                (void);
    void             AddProperty          (const QString &Name, const QVariant &Value);
    void             End                  (void);

  private:
    quint64          BinaryFromVariant    (const QString &Name, const QVariant &Value);
    quint64          BinaryFromStringList (const QString &Name, const QStringList &Value);
    quint64          BinaryFromArray      (const QString &Name, const QVariantList &Value);
    quint64          BinaryFromMap        (const QString &Name, const QVariantMap &Value);
    quint64          BinaryFromHash       (const QString &Name, const QVariantHash &Value);
    quint64          BinaryFromQString    (const QString &Value);
    void             BinaryFromUInt       (quint64 Value);
    void             CountObjects         (quint64 &Count, const QVariant &Value);

  private:
    quint8                 m_referenceSize;
    QTextCodec            *m_codec;
    QList<quint64>         m_objectOffsets;
    QHash<QString,quint64> m_strings;
};

#endif // TORCBINARYPLISTSERIALISER_H
