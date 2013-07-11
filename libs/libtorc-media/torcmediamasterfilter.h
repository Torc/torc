#ifndef TORCMEDIAMASTERFILTER_H
#define TORCMEDIAMASTERFILTER_H

// Qt
#include <QSortFilterProxyModel>

// Torc
#include "torcmediamaster.h"
#include "torcmediaexport.h"

class TORC_MEDIA_PUBLIC TorcMediaMasterFilter : public QSortFilterProxyModel
{
    Q_OBJECT

  public:
    TorcMediaMasterFilter();
    virtual ~TorcMediaMasterFilter();

    Q_PROPERTY (QString  textFilter   READ GetTextFilter   WRITE SetTextFilter   NOTIFY textFilterChanged)
    Q_PROPERTY (bool     filterByName READ GetFilterByName WRITE SetFilterByName NOTIFY filterByNameChanged)

    Q_INVOKABLE void     SetSortOrder        (Qt::SortOrder Order, int Column);
    Q_INVOKABLE void     SetMediaTypeFilter  (TorcMedia::MediaType Type, bool Enabled);

  signals:
    void                 textFilterChanged   (const QString &Text);
    void                 filterByNameChanged (bool Value);

  public slots:
    QString              GetTextFilter       (void);
    bool                 GetFilterByName     (void);
    void                 SetTextFilter       (const QString &Text);
    void                 SetFilterByName     (bool Value);
    void                 SourceChanged       (void);
    TorcMedia*           GetChildByIndex     (int Index) const;

  protected:
    bool                 filterAcceptsRow    (int Row, const QModelIndex &Parent) const;
    bool                 lessThan            (const QModelIndex &Left, const QModelIndex &Right) const;

  private:
    TorcMediaMaster     *m_model;
    int                  m_currentSortColumn;
    int                  m_mediaTypeFilter;
    QString              textFilter;
    bool                 filterByName;
};

#endif // TORCMEDIAMASTERFILTER_H
