/* Class TorcMediaMasterFilter
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

// Qt
#include "torclogging.h"
#include "torcmediamaster.h"
#include "torcmediamasterfilter.h"

/*! \class TorcMediaMasterFilter
 *  \brief A configurable filter proxy
 *
 * \todo Filtering is wrong when columns have been rearranged in TableView
*/

TorcMediaMasterFilter::TorcMediaMasterFilter()
  : QSortFilterProxyModel(),
    m_model(NULL),
    m_currentSortColumn(0),
    m_mediaTypeFilter(TorcMedia::AllTypes),
    textFilter(QString()),
    filterByName(true)
{
    setDynamicSortFilter(true);
    connect(this, SIGNAL(sourceModelChanged()), this, SLOT(SourceChanged()));
}

TorcMediaMasterFilter::~TorcMediaMasterFilter()
{
}

void TorcMediaMasterFilter::SetSortOrder(Qt::SortOrder Order, int Column)
{
    m_currentSortColumn = Column;
    sort(0, Order);
}

void TorcMediaMasterFilter::SetMediaTypeFilter(TorcMedia::MediaType Type, bool Enabled)
{
    int oldfilter = m_mediaTypeFilter;

    if (Enabled)
        m_mediaTypeFilter |= Type;
    else
        m_mediaTypeFilter &= ~Type;

    if (m_mediaTypeFilter != oldfilter)
        invalidateFilter();
}

QString TorcMediaMasterFilter::GetTextFilter(void)
{
    return textFilter;
}

bool TorcMediaMasterFilter::GetFilterByName(void)
{
    return filterByName;
}

void TorcMediaMasterFilter::SetTextFilter(const QString &Text)
{
    if (Text != textFilter)
    {
        textFilter = Text;
        invalidateFilter();
        emit textFilterChanged(textFilter);
    }
}

void TorcMediaMasterFilter::SetFilterByName(bool Value)
{
    if (filterByName != Value)
    {
        filterByName = Value;
        invalidateFilter();
        emit filterByNameChanged(filterByName);
    }
}

void TorcMediaMasterFilter::SourceChanged(void)
{
    m_model = static_cast<TorcMediaMaster*>(sourceModel());
}

bool TorcMediaMasterFilter::filterAcceptsRow(int Row, const QModelIndex &Parent) const
{
    if (m_model)
    {
        TorcMedia* item = m_model->GetChildByIndex(Row);

        if (item)
        {
            if (!(item->GetMediaType() & m_mediaTypeFilter))
                return false;

            if (!textFilter.isEmpty())
            {
                if (filterByName)
                    return item->GetName().contains(textFilter);

                return item->GetURL().contains(textFilter);
            }

            return true;
        }
    }

    return false;
}

bool TorcMediaMasterFilter::lessThan(const QModelIndex &Left, const QModelIndex &Right) const
{
    if (m_model)
    {
        TorcMedia* left  = m_model->GetChildByIndex(Left.row());
        TorcMedia* right = m_model->GetChildByIndex(Right.row());

        if (left && right)
        {
            if (m_currentSortColumn == 0)
                return QString::localeAwareCompare(left->GetName(), right->GetName()) < 0;
            else if (m_currentSortColumn == 1)
                return QString::localeAwareCompare(left->GetURL(), right->GetURL()) < 0;
            else if (m_currentSortColumn == 2)
                return left->GetMediaType() < right->GetMediaType();
        }
    }

    return false;
}
