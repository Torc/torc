#ifndef TORCSETTING_H
#define TORCSETTING_H

// Qt
#include <QSet>
#include <QMutex>
#include <QObject>
#include <QVariant>
#include <QStringList>

// Torc
#include "torccoreexport.h"
#include "torcreferencecounted.h"

class TORC_CORE_PUBLIC TorcSetting : public QObject, public TorcReferenceCounter
{
    Q_OBJECT

  public:
    enum Type
    {
        Checkbox
    };

  public:
    TorcSetting(TorcSetting *Parent, const QString &DBName, const QString &UIName, bool Persistent, const QVariant &Default);

  public:
    QVariant::Type       GetStorageType       (void);
    Type                 GetSettingType       (void);
    void                 AddChild             (TorcSetting *Child);
    void                 RemoveChild          (TorcSetting *Child);
    void                 Remove               (void);
    TorcSetting*         FindChild            (const QString &Child);
    QSet<TorcSetting*>   GetChildren          (void);

  public slots:
    void                 SetTrue              (void);
    void                 SetFalse             (void);
    void                 SetValue             (const QVariant &Value);
    bool                 IsActive             (void);
    void                 SetActive            (bool Value);
    void                 SetActiveThreshold   (int  Threshold);
    void                 SetDescription       (const QString &Description);
    void                 SetHelpText          (const QString &HelpText);

    QVariant             GetValue             (void);
    QString              GetName              (void);
    QString              GetDescription       (void);
    QString              GetHelpText          (void);

  signals:
    void                 ValueChanged         (int  Value);
    void                 ValueChanged         (bool Value);
    void                 ValueChanged         (QString Value);
    void                 ValueChanged         (QStringList Value);
    void                 SettingActivated     (void);
    void                 SettingDeactivated   (void);
    void                 Removed              (void);

  protected:
    virtual             ~TorcSetting();

  protected:
    TorcSetting         *m_parent;
    Type                 m_type;
    bool                 m_persistent;
    QString              m_dbName;
    QString              m_uiName;
    QString              m_description;
    QString              m_helpText;
    QVariant             m_value;
    QVariant             m_default;

  private:
    int                  m_active;
    int                  m_activeThreshold;
    QSet<TorcSetting*>   m_children;
    QMutex              *m_childrenLock;
};

class TORC_CORE_PUBLIC TorcSettingGroup : public TorcSetting
{
  public:
    TorcSettingGroup(TorcSetting *Parent, const QString &UIName);
};

#endif // TORCSETTING_H
