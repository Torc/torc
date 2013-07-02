#ifndef TORCSETTING_H
#define TORCSETTING_H

// Qt
#include <QSet>
#include <QMutex>
#include <QVariant>
#include <QStringList>
#include <QAbstractListModel>

// Torc
#include "torccoreexport.h"
#include "torcreferencecounted.h"

class TORC_CORE_PUBLIC TorcSetting : public QAbstractListModel, public TorcReferenceCounter
{
    Q_OBJECT
    Q_ENUMS(Type)

  public:
    enum Type
    {
        Checkbox,
        Integer
    };

  public:
    TorcSetting(TorcSetting *Parent, const QString &DBName, const QString &UIName,
                Type SettingType, bool Persistent, const QVariant &Default);

  public:
    Q_PROPERTY (QVariant m_value       READ GetValue()       NOTIFY ValueChanged()       )
    Q_PROPERTY (QString  m_uiName      READ GetName()        NOTIFY NameChanged()        )
    Q_PROPERTY (QString  m_description READ GetDescription() NOTIFY DescriptionChanged() )
    Q_PROPERTY (QString  m_helpText    READ GetHelpText()    NOTIFY HelpTextChanged()    )
    Q_PROPERTY (QVariant m_default     READ GetDefault()     NOTIFY DefaultChanged()     )
    Q_PROPERTY (bool     m_persistent  READ GetPersistent()  NOTIFY PersistentChanged()  )
    Q_PROPERTY (int      m_isActive    READ IsActive()       NOTIFY ActiveChanged()      )
    Q_PROPERTY (Type     m_type        READ GetSettingType   NOTIFY TypeChanged()        )

    // QAbstractListModel
    int                    rowCount             (const QModelIndex &Parent = QModelIndex()) const;
    QVariant               data                 (const QModelIndex &Index, int Role) const;
    QHash<int,QByteArray>  roleNames            (void) const;

  public:
    QVariant::Type         GetStorageType       (void);
    void                   AddChild             (TorcSetting *Child);
    void                   RemoveChild          (TorcSetting *Child);
    void                   Remove               (void);
    TorcSetting*           FindChild            (const QString &Child, bool Recursive = false);
    QSet<TorcSetting*>     GetChildren          (void);

  public slots:
    void                   SetValue             (const QVariant &Value);
    void                   SetRange             (int Begin, int End, int Step);
    bool                   IsActive             (void);
    void                   SetActive            (bool Value);
    void                   SetActiveThreshold   (int  Threshold);
    void                   SetDescription       (const QString &Description);
    void                   SetHelpText          (const QString &HelpText);

    QVariant               GetValue             (void);
    QString                GetName              (void);
    QString                GetDescription       (void);
    QString                GetHelpText          (void);
    QVariant               GetDefault           (void);
    bool                   GetPersistent        (void);
    Type                   GetSettingType       (void);
    TorcSetting*           GetChildByIndex      (int Index);

    // Checkbox
    void                   SetTrue              (void);
    void                   SetFalse             (void);

    // Integer
    int                    GetBegin             (void);
    int                    GetEnd               (void);
    int                    GetStep              (void);

  signals:
    void                   NameChanged          (const QString  &Name);    // QML compatabiliy only
    void                   DescriptionChanged   (const QString  &Name);    // QML compatabiliy only
    void                   HelpTextChanged      (const QString  &Name);    // QML compatabiliy only
    void                   DefaultChanged       (const QVariant &Default); // QML compatabiliy only
    void                   PersistentChanged    (bool Persistent);         // QML compatabiliy only
    void                   TypeChanged          (Type NewType);            // QML compatabiliy only
    void                   ValueChanged         (const QVariant &Value);
    void                   ValueChanged         (int  Value);
    void                   ValueChanged         (bool Value);
    void                   ValueChanged         (QString Value);
    void                   ValueChanged         (QStringList Value);
    void                   ActiveChanged        (bool Active);
    void                   Removed              (void);

  protected:
    virtual               ~TorcSetting();

  protected:
    TorcSetting           *m_parent;
    Type                   m_type;
    bool                   m_persistent;
    QString                m_dbName;
    QString                m_uiName;
    QString                m_description;
    QString                m_helpText;
    QVariant               m_value;
    QVariant               m_default;

    // Integer
    int                    m_begin;
    int                    m_end;
    int                    m_step;

  private:
    bool                   m_isActive;
    int                    m_active;
    int                    m_activeThreshold;
    QList<TorcSetting*>    m_children;
    QMutex                *m_childrenLock;
};

class TORC_CORE_PUBLIC TorcSettingGroup : public TorcSetting
{
  public:
    TorcSettingGroup(TorcSetting *Parent, const QString &UIName);
};

#endif // TORCSETTING_H
