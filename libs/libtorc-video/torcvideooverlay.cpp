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

// FFmpeg/libbluray
extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
}

#define MAX_QUEUED_OVERLAYS 200

/*! \class TorcVideoOverlayItem
 *  \brief A wrapper around different types of subtitles, captions, menus and other video graphics.
 *
 * \todo Add bluray overlays.
 * \todo Now that stream index has been added, can probably remove language, flags etc
*/

///brief Construct a TorcVideoOverlayItem that wraps an AVSubtitle.
TorcVideoOverlayItem::TorcVideoOverlayItem(void *Buffer, int Index, QLocale::Language Language, int Flags, bool FixPosition)
  : m_valid(false),
    m_streamIndex(Index),
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
    // we need the stream index to fetch metadata/attachments etc at a later time
    if (Index < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, "Invalid overlay - invalid stream index");
        return;
    }

    // is this a valid AVSubtitle?
    AVSubtitle *subtitle = static_cast<AVSubtitle*>(Buffer);
    if (!subtitle)
    {
        LOG(VB_GENERAL, LOG_ERR, "Invalid overlay item (buffer is not AVSubtitle)");
        return;
    }

    m_startPts = subtitle->start_display_time;
    m_endPts   = subtitle->end_display_time;
    m_valid    = true;
}

///brief Construct a TorcVideoOverlayItem that wraps a list of bd_overlay_s structures.
TorcVideoOverlayItem::TorcVideoOverlayItem(QList<bd_overlay_s *> *Overlays)
  : m_valid(false),
    m_streamIndex(-1),
    m_type(Subtitle),
    m_bufferType(BDOverlay),
    m_flags(0),
    m_language(QLocale::English),
    m_subRectType(SubRectVideo),
    m_displayRect(),
    m_displaySubRect(),
    m_fixRect(false),
    m_startPts(AV_NOPTS_VALUE),
    m_endPts(AV_NOPTS_VALUE),
    m_buffer((void*)Overlays)
{
    if (!Overlays->isEmpty())
    {
        m_type = Overlays->at(0)->plane == 0 ? Graphics : Menu;
        m_valid = m_buffer;
    }
}

///brief Construct a TorcVideoOverlayItem that wraps a list of bd_argb_overlay_s structures.
TorcVideoOverlayItem::TorcVideoOverlayItem(QList<bd_argb_overlay_s *> *Overlays)
  : m_valid(false),
    m_streamIndex(-1),
    m_type(Subtitle),
    m_bufferType(BDARGBOverlay),
    m_flags(0),
    m_language(QLocale::English),
    m_subRectType(SubRectVideo),
    m_displayRect(),
    m_displaySubRect(),
    m_fixRect(false),
    m_startPts(AV_NOPTS_VALUE),
    m_endPts(AV_NOPTS_VALUE),
    m_buffer((void*)Overlays)
{
    if (!Overlays->isEmpty())
    {
        m_type = Overlays->at(0)->plane == 0 ? Graphics : Menu;
        m_valid = m_buffer;
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
        else if (m_bufferType == BDOverlay)
        {
            QList<bd_overlay_s*> *overlays = static_cast<QList<bd_overlay_s*> *>(m_buffer);

            if (overlays)
            {
                while (!overlays->isEmpty())
                {
                    bd_overlay_s* overlay = overlays->takeFirst();

                    if (overlay->palette)
                        delete [] overlay->palette;

                    if (overlay->img)
                        delete [] overlay->img;

                    delete overlay;
                }

                delete overlays;
            }
        }
        else if (m_bufferType == BDARGBOverlay)
        {
            QList<bd_argb_overlay_s*> *overlays = static_cast<QList<bd_argb_overlay_s*> *>(m_buffer);

            if (overlays)
            {
                while (!overlays->isEmpty())
                {
                    bd_argb_overlay_s* overlay = overlays->takeFirst();

                    if (overlay->argb)
                        delete [] overlay->argb;

                    delete overlay;
                }

                delete overlays;
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "Uknown buffer type - leaking");
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
 *
 * /todo TorcVideoOverlayItem needs a rethink.
 */
TorcVideoOverlay::TorcVideoOverlay()
  : m_ignoreOverlays(true),
    m_overlaysLock(new QMutex())
{
}

TorcVideoOverlay::~TorcVideoOverlay()
{
    ClearQueuedOverlays(/*TORC_ALL_OVERLAYS*/);
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

    if (Item)
    {
        if (Item->m_bufferType == TorcVideoOverlayItem::BDOverlay ||
            Item->m_bufferType == TorcVideoOverlayItem::BDARGBOverlay)
        {
            m_menuOverlays.append(Item);
        }
        else
        {
            // NB we use insertMulti to handle duplicated keys (i.e. same pts)
            m_timedOverlays.insertMulti(Item->m_startPts, Item);
        }
    }

    if (m_timedOverlays.size() > MAX_QUEUED_OVERLAYS)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("%1 overlays queued - deleting the oldest").arg(m_timedOverlays.size()));

        while (!m_timedOverlays.isEmpty() && m_timedOverlays.size() > MAX_QUEUED_OVERLAYS)
        {
            QMap<qint64, TorcVideoOverlayItem*>::iterator it = m_timedOverlays.begin();
            delete it.value();
            m_timedOverlays.erase(it);
        }
    }
}

///brief Clear queued overlays that match Type.
void TorcVideoOverlay::ClearQueuedOverlays(int Type /*=TORC_ALL_OVERLAYS*/)
{
    QMutexLocker locker(m_overlaysLock);

    QMutableListIterator<TorcVideoOverlayItem*> i(m_menuOverlays);
    while (i.hasNext())
    {
        i.next();
        if (i.value()->m_type & Type)
        {
            delete i.value();
            i.remove();
        }
    }

    QMutableMapIterator<qint64, TorcVideoOverlayItem*> it(m_timedOverlays);
    while (it.hasNext())
    {
        it.next();
        if (it.value()->m_type & Type)
        {
            delete it.value();
            it.remove();
        }
    }
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

    // create the list - all menu (non-timed) overlays first
    while (!m_menuOverlays.isEmpty())
        overlays.append(m_menuOverlays.takeFirst());

    // and then timed overlays that match the pts
    QMap<qint64, TorcVideoOverlayItem*>::const_iterator it = m_timedOverlays.constBegin();
    for ( ; it != m_timedOverlays.constEnd(); ++it)
    {
        if (it.key() > VideoPts)
            break;
        overlays.append(it.value());
    }

    // and remove them from the master list (this should work fine with duplicated pts keys)
    foreach (TorcVideoOverlayItem* overlay, overlays)
        m_timedOverlays.remove(overlay->m_startPts);

    // and pass them back
    return overlays;
}

