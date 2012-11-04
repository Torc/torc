#ifndef TORCPLIST_H
#define TORCPLIST_H

// Qt
#include <QVariant>
#include <QXmlStreamWriter>

// Export
#include "torccoreexport.h"

class TORC_CORE_PUBLIC TorcPList
{
  public:
    TorcPList       (const QByteArray &data);

    QVariant        GetValue(const QString &Key);
    QString         ToString(void);
    bool            ToXML(QIODevice *Device);

  private:
    void            ParseBinaryPList  (const QByteArray &Data);
    QVariant        ParseBinaryNode   (quint64 Num);
    QVariantMap     ParseBinaryDict   (quint8 *Data);
    QList<QVariant> ParseBinaryArray  (quint8 *Data);
    QVariant        ParseBinaryString (quint8 *Data);
    QVariant        ParseBinaryReal   (quint8 *Data);
    QVariant        ParseBinaryDate   (quint8 *Data);
    QVariant        ParseBinaryData   (quint8 *Data);
    QVariant        ParseBinaryUnicode(quint8 *Data);
    QVariant        ParseBinaryUInt   (quint8 **Data);
    quint64         GetBinaryCount    (quint8 **Data);
    quint64         GetBinaryUInt     (quint8 *Data, quint64 Size);
    quint8*         GetBinaryObject   (quint64 Num);

    bool            ToXML             (const QVariant &Data, QXmlStreamWriter &XML);
    void            DictToXML         (const QVariant &Data, QXmlStreamWriter &XML);
    void            ArrayToXML        (const QVariant &Data, QXmlStreamWriter &XML);

  private:
    QVariant        m_result;
    quint8         *m_data;
    quint8         *m_offsetTable;
    quint64         m_rootObj;
    quint64         m_numObjs;
    quint8          m_offsetSize;
    quint8          m_parmSize;
    QTextCodec     *m_codec;
};

#endif // TORCPLIST_H
