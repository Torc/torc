#ifndef TORCMIME_H
#define TORCMIME_H

// Qt
#include <QIODevice>
#include <QString>

class TorcMime
{
  public:
    static QString MimeTypeForData            (const QByteArray &Data);
    static QString MimeTypeForData            (QIODevice *Device);
    static QString MimeTypeForFileNameAndData (const QString &FileName, QIODevice *Device);
    static QString MimeTypeForFileNameAndData (const QString &FileName, const QByteArray &Data);
    static QString MimeTypeForName            (const QString &Name);
    static QString MimeTypeForUrl             (const QUrl &Url);
    static QStringList MimeTypeForFileName    (const QString &FileName);
};

#endif // TORCMIME_H
