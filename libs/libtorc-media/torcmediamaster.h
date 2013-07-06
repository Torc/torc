#ifndef TORCMEDIAMASTER_H
#define TORCMEDIAMASTER_H

// Qt
#include <QAbstractListModel>

// Torc
#include "torcmediaexport.h"
#include "torcmedia.h"

class TORC_MEDIA_PUBLIC TorcMediaMaster : public QAbstractListModel
{
    Q_OBJECT

  public:
    TorcMediaMaster();
    virtual ~TorcMediaMaster();
    
    // QAbstractListModel
    QVariant                   data          (const QModelIndex &Index, int Role) const;
    QHash<int,QByteArray>      roleNames     (void) const;
    int                        rowCount      (const QModelIndex &Parent = QModelIndex()) const;

  protected:
    bool                       event         (QEvent *Event);

    QList<TorcMedia*> m_media;
};

extern TORC_MEDIA_PUBLIC TorcMediaMaster *gTorcMediaMaster;

#endif // TORCMEDIAMASTER_H
