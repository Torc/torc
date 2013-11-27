/* Class TorcVideoOverlay/Type
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
#include "torclogging.h"
#include "torcvideooverlay.h"

// FFmpeg
extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
}

#define MAX_QUEUED_OVERLAYS 50

/*! \class TorcVideoOverlayItem
 *  \brief A wrapper around different types of subtitles, captions, menus and other video graphics.
 *
 * \todo Add bluray overlays.
*/
TorcVideoOverlayItem::TorcVideoOverlayItem(void *Buffer, QLocale::Language Language, int Flags, bool FixPosition)
  : m_valid(false),
    m_type(Subtitle),
    m_bufferType(FFmpegSubtitle),
    m_flags(Flags),
    m_language(Language),
    m_subRectType(SubRectVideo),
    m_displayRect(),
    m_displaySubRect(),
    m_fixRect(FixPosition),
    m_startPts(AV_NOPTS_VALUE),
    m_endPts(AV_NOPTS_VALUE),
    m_buffer((void*)Buffer)
{
    AVSubtitle *subtitle = static_cast<AVSubtitle*>(Buffer);
    if (subtitle)
    {
        m_startPts = subtitle->pts + subtitle->start_display_time;
        m_endPts   = subtitle->pts + subtitle->end_display_time;
        m_valid    = true;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Invalid overlay item (buffer is not AVSubtitle)");
    }
}

TorcVideoOverlayItem::TorcVideoOverlayItem()
{
}

/*! \brief Delete this overlay
 *
 * Destruction of the underlying buffer is dependant on the buffer type. Handle with care.
*/
TorcVideoOverlayItem::~TorcVideoOverlayItem()
{
    if (m_buffer)
    {
        if (m_bufferType == FFmpegSubtitle)
        {
            AVSubtitle *subtitle = static_cast<AVSubtitle*>(m_buffer);

            if (subtitle)
            {
                for (uint i = 0; i < subtitle->num_rects; ++i)
                {
                     av_free(subtitle->rects[i]->pict.data[0]);
                     av_free(subtitle->rects[i]->pict.data[1]);
                }

                if (subtitle->num_rects > 0)
                    av_free(subtitle->rects);
            }
        }

        m_buffer = NULL;
    }
}

///brief The overlay is complete and its buffer type is known.
bool TorcVideoOverlayItem::IsValid(void)
{
    return m_valid;
}

/*! \class TorcVideoOverlay
 *  \brief Maintains an ordered list of queued overlay items, sorted by presentation timestamp.
 *
 * TorcVideoOverlay is intended to be incorporated into another object (e.g. a media player).
 *
 * \note Overlays are disabled by default. Set m_ignoreOverlays to false to enable.
 *
 * \sa TorcVideoOverlayItem
 */
TorcVideoOverlay::TorcVideoOverlay()
  : m_ignoreOverlays(true),
    m_overlaysLock(new QMutex())
{
}

TorcVideoOverlay::~TorcVideoOverlay()
{
    ClearAllOverlays();
}

/*! \brief Add an overlay to the presentation queue.
 *
 * Overlays are queued on the basis of their starting timestamp. If they are not cleared
 * quickly enough (if the player is stalled), the oldest will be deleted without being
 * presented.
*/
void TorcVideoOverlay::AddOverlay(TorcVideoOverlayItem *Item)
{
    // by default overlays are ignored and deleted
    if (m_ignoreOverlays)
    {
        delete Item;
        return;
    }

    QMutexLocker locker(m_overlaysLock);

    // NB we use insertMulti to handle duplicated keys (i.e. same pts)
    if (Item)
        m_overlays.insertMulti(Item->m_startPts, Item);

    if (m_overlays.size() > MAX_QUEUED_OVERLAYS)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("%1 overlays queued - deleting the oldest").arg(m_overlays.size()));

        while (!m_overlays.isEmpty() && m_overlays.size() > MAX_QUEUED_OVERLAYS)
        {
            QMap<qint64, TorcVideoOverlayItem*>::iterator it = m_overlays.begin();
            delete it.value();
            m_overlays.erase(it);
        }
    }
}

///brief Clear all existing overlays.
void TorcVideoOverlay::ClearAllOverlays(void)
{
    QMutexLocker locker(m_overlaysLock);

    QMap<qint64, TorcVideoOverlayItem*>::const_iterator it = m_overlays.constBegin();
    for ( ; it != m_overlays.constEnd(); ++it)
        delete it.value();

    m_overlays.clear();
}

/*! \brief Return an ordered list of overlays that are older than VideoPts.
 *
 * The caller takes ownership of the returned TorcVideoOverlayItem's and must
 * delete them when they have been processed.
 *
 * \todo Check wrap behaviour here.
*/
QList<TorcVideoOverlayItem*> TorcVideoOverlay::GetNewOverlays(qint64 VideoPts)
{
    QMutexLocker locker(m_overlaysLock);

    QList<TorcVideoOverlayItem*> overlays;

    // create the list
    QMap<qint64, TorcVideoOverlayItem*>::const_iterator it = m_overlays.constBegin();
    for ( ; it != m_overlays.constEnd(); ++it)
    {
        if (it.key() > VideoPts)
            break;
        overlays.append(it.value());
    }

    // and remove them from the master list (this should work fine with duplicated pts keys)
    foreach (TorcVideoOverlayItem* overlay, m_overlays)
        m_overlays.remove(overlay->m_startPts);

    // and pass them back
    return overlays;
}

