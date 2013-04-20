/* Class TorcSetting
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2013
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Torc
#include "torclocalcontext.h"
#include "torcsetting.h"

/*! \class TorcSetting
 *  \brief A wrapper around a database setting.
 *
 * TorcSetting acts as a wrapper around a persistent (stored in a database) or temporary
 * (system/state dependant) setting.
 *
 * Setting storage types are currently limited to Int, Bool, String or StringList but
 * could be extended to any QVariant supported type that can be converted to a string
 * and hence easily stored within the database.
 *
 * The setting type describes the setting's behaviour.
 *
 * TorcSetting additionally acts as a state machine. It can be activated by other settings
 * and QObjects and can notify other objects of any change to its value.
 *
 * A class can create and connect a number of TorcSettings to encapsulate the desired
 * setting behaviour and allow the setting structure to be presented and manipulated by
 * an appropriate user facing interface.
 *
 * \sa TorcUISetting
 */

TorcSetting::TorcSetting(TorcSetting *Parent, const QString &DBName, const QString &UIName,
                         bool Persistent, const QVariant &Default)
  : QObject(),
    m_parent(Parent),
    m_type(Checkbox),
    m_persistent(Persistent),
    m_dbName(DBName),
    m_uiName(UIName),
    m_default(Default),
    m_active(0),
    m_activeThreshold(0),
    m_childrenLock(new QMutex(QMutex::Recursive))
{
    setObjectName(DBName);

    if (m_parent)
        m_parent->AddChild(this);

    QVariant::Type type = m_default.type();

    if (type == QVariant::Int)
    {
        m_value = m_persistent ? gLocalContext->GetSetting(m_dbName, (int)m_default.toInt()) : m_default.toInt();
    }
    else if (type == QVariant::Bool)
    {
        m_value = m_persistent ? gLocalContext->GetSetting(m_dbName, (bool)m_default.toBool()) : m_default.toBool();
        LOG(VB_GENERAL, LOG_INFO, QString("%1 loaded %2").arg(objectName()).arg(m_value.toString()));
    }
    else if (type == QVariant::String)
    {
        m_value = m_persistent ? gLocalContext->GetSetting(m_dbName, (QString)m_default.toString()) : m_default.toString();
    }
    else if (type == QVariant::StringList)
    {
        if (m_persistent)
        {
            QString value = gLocalContext->GetSetting(m_dbName, (QString)m_default.toString());
            m_value = QVariant(value.split(","));
        }
        else
        {
            m_value = m_default.toStringList();
        }
    }
    else
    {
        if (type != QVariant::Invalid)
            LOG(VB_GENERAL, LOG_ERR, QString("Unsupported setting data type: %1").arg(type));
        m_value = QVariant();
    }
}

TorcSetting::~TorcSetting()
{
    delete m_childrenLock;
}

QVariant::Type TorcSetting::GetStorageType(void)
{
    return m_default.type();
}

TorcSetting::Type TorcSetting::GetSettingType(void)
{
    return m_type;
}

void TorcSetting::AddChild(TorcSetting *Child)
{
    if (Child)
    {
        {
            QMutexLocker locker(m_childrenLock);
            m_children.insert(Child);
        }

        Child->UpRef();
    }
}

void TorcSetting::RemoveChild(TorcSetting *Child)
{
    if (Child)
    {
        {
            QMutexLocker locker(m_childrenLock);
            m_children.remove(Child);
        }

        Child->DownRef();
    }
}

void TorcSetting::Remove(void)
{
    if (m_parent)
        m_parent->RemoveChild(this);

    emit Removed();
}

TorcSetting* TorcSetting::FindChild(const QString &Child)
{
    QMutexLocker locker(m_childrenLock);

    foreach (TorcSetting* setting, m_children)
        if (setting->objectName() == Child)
            return setting;

    return NULL;
}

QSet<TorcSetting*> TorcSetting::GetChildren(void)
{
    QSet<TorcSetting*> result;

    m_childrenLock->lock();
    foreach (TorcSetting* setting, m_children)
    {
        result << setting;
        setting->UpRef();
    }
    m_childrenLock->unlock();

    return result;
}

bool TorcSetting::IsActive(void)
{
    return m_active >= m_activeThreshold;
}

QString TorcSetting::GetName(void)
{
    return m_uiName;
}

QString TorcSetting::GetDescription(void)
{
    return m_description;
}

QString TorcSetting::GetHelpText(void)
{
    return m_helpText;
}

void TorcSetting::SetActive(bool Value)
{
    bool wasactive = IsActive();
    m_active += Value ? 1 : -1;

    if (wasactive != IsActive())
    {
        if (IsActive())
            emit SettingActivated();
        else
            emit SettingDeactivated();
    }
}

void TorcSetting::SetActiveThreshold(int Threshold)
{
    bool wasactive = IsActive();
    m_activeThreshold = Threshold - 1;

    if (wasactive != IsActive())
    {
        if (IsActive())
            emit SettingActivated();
        else
            emit SettingDeactivated();
    }
}

void TorcSetting::SetTrue(void)
{
    LOG(VB_GENERAL, LOG_INFO, "True");
    if (m_default.type() == QVariant::Bool)
        SetValue(QVariant((bool)true));
}

void TorcSetting::SetFalse(void)
{
    LOG(VB_GENERAL, LOG_INFO, "False");
    if (m_default.type() == QVariant::Bool)
        SetValue(QVariant((bool)false));
}

void TorcSetting::SetValue(const QVariant &Value)
{
    LOG(VB_GENERAL, LOG_INFO, QString("Set %1: active %2 value %3").arg(objectName()).arg(IsActive()).arg(Value.toString()));

    if (m_value == Value || !IsActive())
        return;

    m_value = Value;

    QVariant::Type type = m_default.type();

    if (type == QVariant::Int)
    {
        int value = m_value.toInt();
        if (m_persistent)
            gLocalContext->SetSetting(m_dbName, (int)value);
        emit ValueChanged(value);
    }
    else if (type == QVariant::Bool)
    {
        bool value = m_value.toBool();
        if (m_persistent)
            gLocalContext->SetSetting(m_dbName, (bool)value);
        LOG(VB_GENERAL, LOG_INFO, QString("%1 set %2").arg(objectName()).arg(value));
        emit ValueChanged(value);
    }
    else if (type == QVariant::String)
    {
        QString value = m_value.toString();
        if (m_persistent)
            gLocalContext->SetSetting(m_dbName, value);
        emit ValueChanged(value);
    }
    else if (type == QVariant::StringList)
    {
        QStringList value = m_value.toStringList();
        if (m_persistent)
            gLocalContext->SetSetting(m_dbName, value.join(","));
        emit ValueChanged(value);
    }
}

void TorcSetting::SetDescription(const QString &Description)
{
    m_description = Description;
}

void TorcSetting::SetHelpText(const QString &HelpText)
{
    m_helpText = HelpText;
}

QVariant TorcSetting::GetValue(void)
{
    return m_value;
}

/*! \class TorcSettingGroup
 *  \brief High level group of related settings
 *
 * \sa TorcSetting
*/

TorcSettingGroup::TorcSettingGroup(TorcSetting *Parent, const QString &UIName)
  : TorcSetting(Parent, UIName, UIName, false, QVariant())
{
}