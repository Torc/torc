/* Class TorcBlurayUIHandler
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
#include "torcavutils.h"
#include "videodecoder.h"
#include "videoplayer.h"
#include "torcvideooverlay.h"
#include "torcblurayuihandler.h"

extern "C" {
#include "libbluray/src/libbluray/bluray-version.h"
#include "libbluray/src/util/log_control.h"
#include "libbluray/src/libbluray/register.h"
#include "libbluray/src/libbluray/bdnav/bdparse.h"
#include "libbluray/src/libbluray/bdnav/meta_data.h"
#include "libbluray/src/libbluray/keys.h"
}

/*! \class TorcBlurayUIHandler
 *  \brief A subclass of TorcBlurayHandler to implement user menus.
 *
 * \todo Settings for audio language, subtitle language, menu language etc.
 * \todo Setting for using menus.
 * \todo Setting for using BD-J (parent to menu support).
*/
TorcBlurayUIHandler::TorcBlurayUIHandler(TorcBlurayBuffer *Parent, const QString &Path, int *Abort)
  : TorcBlurayHandler(Parent, Path, Abort),
    m_player(NULL),
    m_currentPlayList(-1),
    m_currentPlayItem(0),
    m_ignoreWaits(true),
    m_endOfTitle(false)
{
    m_useMenus = true;

    m_decoder = static_cast<VideoDecoder*>(m_parent->GetParent());
    if (m_decoder)
        m_player = m_decoder->GetPlayerParent();
}

TorcBlurayUIHandler::~TorcBlurayUIHandler()
{
    Close();
}

void TorcBlurayUIHandler::SetIgnoreWaits(bool Ignore)
{
    m_ignoreWaits = Ignore;
}

void OverlayCallback (void *Object, const struct bd_overlay_s * const Overlay)
{
    TorcBlurayUIHandler* handler = static_cast<TorcBlurayUIHandler*>(Object);
    if (handler)
        handler->HandleOverlay(Overlay);
}

void ARGBOverlayCallback(void *Object, const struct bd_argb_overlay_s * const Overlay)
{
    TorcBlurayUIHandler* handler = static_cast<TorcBlurayUIHandler*>(Object);
    if (handler)
        handler->HandleARGBOverlay(Overlay);
}

bool TorcBlurayUIHandler::Open(void)
{
    if (!TorcBlurayHandler::Open())
        return false;

    if (m_useMenus)
    {
        // register overlay callbacks
        bd_register_overlay_proc     (m_bluray, this, OverlayCallback);
        bd_register_argb_overlay_proc(m_bluray, this, ARGBOverlayCallback, NULL);
    }

    return true;
}

void TorcBlurayUIHandler::Close(void)
{
    if (m_useMenus)
    {
        // deregister for overlay callbacks
        bd_register_overlay_proc     (m_bluray, this, NULL);
        //bd_register_argb_overlay_proc(m_bluray, this, NULL, NULL);
    }

    // close overlays
    ClearOverlays();
    ClearARGBOverlays();

    TorcBlurayHandler::Close();
}

int TorcBlurayUIHandler::Read(quint8 *Buffer, qint32 BufferSize)
{
    if (!m_bluray)
        return AVERROR_UNKNOWN;

    if (!m_useMenus)
        return TorcBlurayHandler::Read(Buffer, BufferSize);

    int eventstatus = 0;
    int result = 0;

    while (result == 0)
    {
        BD_EVENT event;
        result = bd_read_ext(m_bluray, (unsigned char *)Buffer, BufferSize, &event);

        if ((eventstatus = HandleBDEvent(event)) < 0)
            return eventstatus;
    }

    return result;

}

//#include "libbluray/src/libbluray/bdnav/navigation.h"

/*! \brief Create an AVInputFormat for the current playlist.
 *
 * Normal probing (via AudioDecoder) fails for bluray buffers because peek is not implemented.
 * We do however know that the required demuxer is 'mpegts' - and this is our default/fallback input format.
 * Certain playlists (typically stillframes and/or VC1 content) are not however successfully scanned via
 * avformat_find_stream_info when we supply an 'empty' AVInputFormat. So we open the playlist file directly
 * (bypassing libbluray and hence leaving the playback state unchanged), probe it and fallback to 'mpegts' if necessary.
*/
AVInputFormat* TorcBlurayUIHandler::GetAVInputFormat(void)
{
    return NULL;

    /*
    if (!m_useMenus)
        return NULL;

    if (m_bluray)
    {
        // this will usually be called when the current playlist is invalid - either because we've just
        // started playback or the last title has finished and we haven't made any more reads yet. We need to poke
        // libbluray... without pulling out any actual media data.

        quint32 oldplaylist = m_currentPlayList;

        BD_EVENT event;
        while (bd_read_ext(m_bluray, NULL, 0, &event) == 0 && event.event != BD_EVENT_NONE)
            HandleBDEvent(event);

        LOG(VB_GENERAL, LOG_INFO, QString("Playlist: old %1 new %2").arg(oldplaylist).arg(m_currentPlayList));

        NAV_TITLE *navtitle = static_cast<NAV_TITLE*>(bd_get_navtitle(m_bluray));
        if (navtitle && navtitle->clip_list.count)
            return NULL;
    }

    return av_find_input_format("mpegts");
    */
}

bd_argb_overlay_s* CopyARGBOverlay(const bd_argb_overlay_s * const Overlay)
{
    bd_argb_overlay_s *overlay = new bd_argb_overlay_s;
    memcpy(overlay, Overlay, sizeof(bd_argb_overlay_s));
    if (overlay->argb)
    {
        // 'compress' images if necessary. TorcOverlayDecoder is optimised for w == stride and some images
        // can be much larger than required.
        if (overlay->stride != overlay->w)
        {
            overlay->argb = new uint32_t[overlay->w * overlay->h];
            uint32_t *dest   = (uint32_t*)overlay->argb;
            uint32_t *source = (uint32_t*)Overlay->argb;
            for (uint16_t y = 0; y < overlay->h; y++, source += Overlay->stride, dest += overlay->w)
                memcpy(dest, source, sizeof(uint32_t) * overlay->w);
            overlay->stride = overlay->w;
        }
        else
        {
            overlay->argb = new uint32_t[overlay->stride * overlay->h];
            memcpy((void*)overlay->argb, Overlay->argb, sizeof(uint32_t) * overlay->stride * overlay->h);
        }
    }

    return overlay;
}

void DeleteOverlay(bd_overlay_s *Overlay)
{
    if (!Overlay)
        return;

    if (Overlay->palette)
        delete [] Overlay->palette;

    if (Overlay->img)
        delete [] Overlay->img;

    delete Overlay;
}

void ClearOverlayQueue(QList<bd_overlay_s*> *Queue)
{
    while (Queue && !Queue->isEmpty())
        DeleteOverlay(Queue->takeFirst());
}

bd_overlay_s* CopyOverlay(const bd_overlay_s * const Overlay)
{
    bd_overlay_s *overlay = new bd_overlay_s;
    memcpy(overlay, Overlay, sizeof(bd_overlay_s));
    if (overlay->palette)
    {
        overlay->palette = new bd_pg_palette_entry_s[256];
        memcpy((void*)overlay->palette, Overlay->palette, 256 * sizeof(BD_PG_PALETTE_ENTRY));
    }

    if (overlay->img)
    {
        // this should use the bd_refcnt_inc/dec api but the symbols aren't being exported
        // correctly, so application linking is failing.
        const BD_PG_RLE_ELEM *raw = overlay->img;
        uint maxsize = overlay->w * overlay->h;

        uint length = 0;
        for (uint i = 0 ; i < maxsize; raw++, length++)
            i += raw->len;

        overlay->img = new BD_PG_RLE_ELEM[length];
        memcpy((void*)overlay->img, Overlay->img, sizeof(BD_PG_RLE_ELEM) * length);
    }

    return overlay;
}

void DeleteARGBOverlay(bd_argb_overlay_s *Overlay)
{
    if (!Overlay)
        return;

    if (Overlay->argb)
        delete [] Overlay->argb;

    delete Overlay;
}

void ClearARGBOverlayQueue(QList<bd_argb_overlay_s*> *Queue)
{
    while (Queue && !Queue->isEmpty())
        DeleteARGBOverlay(Queue->takeFirst());
}

void TorcBlurayUIHandler::ClearOverlays(void)
{
    // clear any queues
    ClearOverlayQueue(&m_overlays[0]);
    ClearOverlayQueue(&m_overlays[1]);

    // close both overlays
    bd_overlay_s *pg = new bd_overlay_s;
    bd_overlay_s *ig = new bd_overlay_s;
    memset(pg, 0, sizeof(bd_overlay_s));
    memset(ig, 0, sizeof(bd_overlay_s));
    pg->cmd = BD_OVERLAY_CLOSE;
    ig->cmd = BD_OVERLAY_CLOSE;
    pg->plane = 0;
    ig->plane = 1;

    QList<bd_overlay_s*> *pglist = new QList<bd_overlay_s*>();
    pglist->append(pg);
    TorcVideoOverlayItem* pgitem = new TorcVideoOverlayItem(pglist);
    QList<bd_overlay_s*> *iglist = new QList<bd_overlay_s*>();
    pglist->append(ig);
    TorcVideoOverlayItem* igitem = new TorcVideoOverlayItem(iglist);

    if (pgitem->IsValid())
        m_player->AddOverlay(pgitem);
    else
        delete pgitem;

    if (igitem->IsValid())
        m_player->AddOverlay(igitem);
    else
        delete igitem;
}

void TorcBlurayUIHandler::ClearARGBOverlays(void)
{
    // clear any queues
    ClearARGBOverlayQueue(&m_argbOverlays[0]);
    ClearARGBOverlayQueue(&m_argbOverlays[1]);

    // close both overlays
    bd_argb_overlay_s *pg = new bd_argb_overlay_s;
    bd_argb_overlay_s *ig = new bd_argb_overlay_s;
    memset(pg, 0, sizeof(bd_argb_overlay_s));
    memset(ig, 0, sizeof(bd_argb_overlay_s));
    pg->cmd = BD_OVERLAY_CLOSE;
    ig->cmd = BD_OVERLAY_CLOSE;
    pg->plane = 0;
    ig->plane = 1;

    QList<bd_argb_overlay_s*> *pglist = new QList<bd_argb_overlay_s*>();
    pglist->append(pg);
    TorcVideoOverlayItem* pgitem = new TorcVideoOverlayItem(pglist);
    QList<bd_argb_overlay_s*> *iglist = new QList<bd_argb_overlay_s*>();
    pglist->append(ig);
    TorcVideoOverlayItem* igitem = new TorcVideoOverlayItem(iglist);

    if (pgitem->IsValid())
        m_player->AddOverlay(pgitem);
    else
        delete pgitem;

    if (igitem->IsValid())
        m_player->AddOverlay(igitem);
    else
        delete igitem;
}

void TorcBlurayUIHandler::HandleOverlay(const bd_overlay_s * const Overlay)
{
    if (!m_player)
        return;

    if (!Overlay)
    {
        LOG(VB_GENERAL, LOG_INFO, "Clear overlays");
        ClearOverlays();
        return;
    }

    // ignore unknown overlays
    if (Overlay->plane > 1)
        return;

    LOG(VB_GENERAL, LOG_DEBUG, QString("Overlay %1: command %2 pts %3").arg(Overlay->plane).arg(Overlay->cmd).arg(Overlay->pts));

    // queue everything except flushes
    if (Overlay->cmd != BD_OVERLAY_FLUSH)
        m_overlays[Overlay->plane].append(CopyOverlay(Overlay));

    // NB BD_OVERLAY_CLOSE is not followed by a flush
    if (Overlay->cmd == BD_OVERLAY_FLUSH || Overlay->cmd == BD_OVERLAY_CLOSE)
    {
        // ensure a BD_OVERLAY_CLOSE command is always the last or only command
        while (!m_overlays[Overlay->plane].isEmpty())
        {
            QList<bd_overlay_s*> *overlays = new QList<bd_overlay_s*>();
            while (!m_overlays[Overlay->plane].isEmpty())
            {
                bd_overlay_s* bdoverlay = m_overlays[Overlay->plane].takeFirst();
                overlays->append(bdoverlay);

                if (bdoverlay->cmd == BD_OVERLAY_CLOSE)
                    break;
            }

            TorcVideoOverlayItem* overlay = new TorcVideoOverlayItem(overlays);

            if (overlay->IsValid())
                m_player->AddOverlay(overlay);
            else
                delete overlay;
        }
    }
}

void TorcBlurayUIHandler::HandleARGBOverlay(const bd_argb_overlay_s * const Overlay)
{
    if (!m_player)
        return;

    if (!Overlay)
    {
        ClearARGBOverlays();
        return;
    }

    // ignore unknown overlays
    if (Overlay->plane > 1)
        return;

    LOG(VB_GENERAL, LOG_DEBUG, QString("ARGBOverlay %1: command %2 pts %3").arg(Overlay->plane).arg(Overlay->cmd).arg(Overlay->pts));

    if (Overlay->cmd != BD_ARGB_OVERLAY_FLUSH)
        m_argbOverlays[Overlay->plane].append(CopyARGBOverlay(Overlay));

    if (Overlay->cmd == BD_ARGB_OVERLAY_FLUSH || Overlay->cmd == BD_ARGB_OVERLAY_CLOSE)
    {
        // ensure a BD_ARGB_OVERLAY_CLOSE command is always the last (or only) command
        while (!m_argbOverlays[Overlay->plane].isEmpty())
        {
            QList<bd_argb_overlay_s*> *overlays = new QList<bd_argb_overlay_s*>();
            while (!m_argbOverlays[Overlay->plane].isEmpty())
            {
                bd_argb_overlay_s* bdoverlay = m_argbOverlays[Overlay->plane].takeFirst();
                overlays->append(bdoverlay);

                if (bdoverlay->cmd == BD_ARGB_OVERLAY_CLOSE)
                    break;
            }

            TorcVideoOverlayItem* overlay = new TorcVideoOverlayItem(overlays);

            if (overlay->IsValid())
                m_player->AddOverlay(overlay);
            else
                delete overlay;
        }
    }
}

/*! \brief Handle mouse click use interaction.
 *
 * \todo Add locking.
 * \todo Ignore if there is no menu support or not in menu.
*/
bool TorcBlurayUIHandler::MouseClicked(qint64 Pts, quint16 X, quint16 Y)
{
    if (!m_useMenus)
        return false;

    if (m_bluray)
    {
        int result = bd_mouse_select(m_bluray, Pts, X, Y);

        LOG(VB_GENERAL, LOG_INFO, QString("Mouse clicked %1+%2 - result %3").arg(X).arg(Y).arg(result));

        if (result > 0)
            (void)bd_user_input(m_bluray, Pts, BD_VK_MOUSE_ACTIVATE);

        return result > 0;
    }

    return false;
}

/*! \brief Handle mouse movements.
 *
 * \todo Add locking.
 * \todo Ignore if there is no menu support or not in menu.
*/
bool TorcBlurayUIHandler::MouseMoved(qint64 Pts, quint16 X, quint16 Y)
{
    if (!m_useMenus)
        return false;

    if (m_bluray)
    {
        int result = bd_mouse_select(m_bluray, Pts, X, Y);
        LOG(VB_GENERAL, LOG_DEBUG, QString("Mouse move %1+%2 - result %3").arg(X).arg(Y).arg(result));
        return result > 0;
    }

    return false;
}

int TorcBlurayUIHandler::HandleBDEvent(BD_EVENT &Event)
{
    if (!m_bluray)
        return AVERROR_UNKNOWN;

    switch (Event.event)
    {
        case BD_EVENT_PLAYLIST:
            {
                LOG(VB_GENERAL, LOG_INFO, QString("EVENT_PLAYLIST %1").arg(Event.param));

                int index = Event.param;

                {
                    QMutexLocker locker(m_currentTitleInfoLock);
                    if (m_currentPlayList == index /*&& !m_endOfTitle*/)
                        break;

                    m_currentPlayList = index;

                    if (m_currentTitleInfo)
                        bd_free_title_info(m_currentTitleInfo);

                    m_currentTitleInfo = bd_get_playlist_info(m_bluray, m_currentPlayList, 0);
                    ProcessTitleInfo();

                    if (m_endOfTitle)
                    {
                        m_endOfTitle = false;
                        // mark the demuxer as waiting (let the player drain existing buffers), return RESET signal
                        // and let the decoder deal with the transition when ready.
                        if (!m_ignoreWaits)
                            return TORC_AVERROR_RESET_NOW;
                        else
                            LOG(VB_GENERAL, LOG_INFO, "Ignoring wait state for end of title/new playlist");
                    }
                    else
                    {
                        if (!m_ignoreWaits)
                        {
                            m_decoder->SetDemuxerState(TorcDecoder::DemuxerWaiting);
                            return TORC_AVERROR_RESET;
                        }
                        else
                        {
                            LOG(VB_GENERAL, LOG_INFO, "Ignoring wait state for new playlist");
                        }
                    }
                }
            }

            break;
        case BD_EVENT_END_OF_TITLE:
            LOG(VB_GENERAL, LOG_INFO, QString("EVENT_END_OF_TITLE"));
            m_endOfTitle = false;//true;
            break;
        case BD_EVENT_PLAYITEM:
            LOG(VB_GENERAL, LOG_INFO, QString("EVENT_PLAYITEM %1").arg(Event.param));
            {
                quint32 oldplayitem = m_currentPlayItem;
                m_currentPlayItem = Event.param;

                if (m_currentPlayItem != oldplayitem)
                {
                    if (!m_ignoreWaits)
                    {
                        m_decoder->SetDemuxerState(TorcDecoder::DemuxerWaiting);
                        return TORC_AVERROR_FLUSH;
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_INFO, "Ignoring wait state for new playitem");
                    }
                }
            }
            break;
        case BD_EVENT_DISCONTINUITY:
            LOG(VB_GENERAL, LOG_INFO, QString("EVENT_DISCONTINUITY %1").arg(Event.param));
            {
                if (!m_ignoreWaits)
                {
                    m_decoder->SetDemuxerState(TorcDecoder::DemuxerWaiting);
                    return TORC_AVERROR_FLUSH;
                }

                LOG(VB_GENERAL, LOG_INFO, "Ignoring wait state for discontinuity");
            }
            break;
        case BD_EVENT_STILL_TIME:
            LOG(VB_GENERAL, LOG_INFO, QString("EVENT_STILL_TIME %1").arg(Event.param));
            {
                if (!m_ignoreWaits)
                {
                    m_decoder->SetDemuxerState(TorcDecoder::DemuxerWaiting);
                    return TORC_AVERROR_IDLE;
                }

                LOG(VB_GENERAL, LOG_INFO, "Ignoring wait state for still frame");
            }
            break;
        case BD_EVENT_IDLE:
            //LOG(VB_GENERAL, LOG_INFO, QString("EVENT_IDLE %1").arg(Event.param));
            return TORC_AVERROR_IDLE;
        case BD_EVENT_NONE:
            break;
        case BD_EVENT_ERROR:
            LOG(VB_GENERAL, LOG_INFO, QString("EVENT_ERROR %1 - closing").arg(Event.param));
            return AVERROR_UNKNOWN;
            break;
        case BD_EVENT_ENCRYPTED:
            LOG(VB_GENERAL,  LOG_ERR,  QString("EVENT_ENCRYPTED - aborting"));
            return AVERROR_UNKNOWN;
            break;
        case BD_EVENT_ANGLE:
            LOG(VB_GENERAL, LOG_INFO, QString("EVENT_ANGLE %1").arg(Event.param));
            break;
        case BD_EVENT_TITLE:
            LOG(VB_GENERAL, LOG_INFO, QString("EVENT_TITLE %1").arg(Event.param));
            break;
        case BD_EVENT_CHAPTER:
            LOG(VB_GENERAL, LOG_INFO, QString("EVENT_CHAPTER %1").arg(Event.param));
            break;
        case BD_EVENT_PLAYMARK:
            LOG(VB_GENERAL, LOG_INFO, QString("EVENT_PLAYMARK %1").arg(Event.param));
            break;
        case BD_EVENT_STILL:
            LOG(VB_GENERAL, LOG_INFO, QString("EVENT_STILL %1").arg(Event.param));
            break;
        case BD_EVENT_SEEK:
            LOG(VB_GENERAL, LOG_INFO, QString("EVENT_SEEK %1").arg(Event.param));
            break;
        case BD_EVENT_AUDIO_STREAM:
            LOG(VB_GENERAL, LOG_INFO, QString("EVENT_AUDIO_STREAM %1").arg(Event.param));
            break;
        case BD_EVENT_IG_STREAM:
            LOG(VB_GENERAL, LOG_INFO, QString("EVENT_IG_STREAM %1").arg(Event.param));
            break;
        case BD_EVENT_PG_TEXTST_STREAM:
            LOG(VB_GENERAL, LOG_INFO, QString("EVENT_PG_TEXTST_STREAM %1").arg(Event.param));
            break;
        case BD_EVENT_SECONDARY_AUDIO_STREAM:
            LOG(VB_GENERAL, LOG_INFO, QString("EVENT_SECONDARY_AUDIO_STREAM %1").arg(Event.param));
            break;
        case BD_EVENT_SECONDARY_VIDEO_STREAM:
            LOG(VB_GENERAL, LOG_INFO, QString("EVENT_SECONDARY_VIDEO_STREAM %1").arg(Event.param));
            break;
        case BD_EVENT_PG_TEXTST:
            LOG(VB_GENERAL, LOG_INFO, QString("EVENT_PG_TEXTST %1").arg(Event.param));
            break;
        case BD_EVENT_SECONDARY_AUDIO:
            LOG(VB_GENERAL, LOG_INFO, QString("EVENT_SECONDARY_AUDIO %1").arg(Event.param));
            break;
        case BD_EVENT_SECONDARY_VIDEO:
            LOG(VB_GENERAL, LOG_INFO, QString("EVENT_SECONDARY_VIDEO %1").arg(Event.param));
            break;
        case BD_EVENT_SECONDARY_VIDEO_SIZE:
            LOG(VB_GENERAL, LOG_INFO, QString("EVENT_SECONDARY_VIDEO_SIZE %1").arg(Event.param));
            break;
        case BD_EVENT_SOUND_EFFECT:
            LOG(VB_GENERAL, LOG_INFO, QString("EVENT_SOUND_EFFECT %1").arg(Event.param));
            break;
        case BD_EVENT_POPUP:
            LOG(VB_GENERAL, LOG_INFO, QString("EVENT_POPUP %1").arg(Event.param));
            break;
        case BD_EVENT_MENU:
            LOG(VB_GENERAL, LOG_INFO, QString("EVENT_MENU %1").arg(Event.param));
            break;
        case BD_EVENT_STEREOSCOPIC_STATUS:
            LOG(VB_GENERAL, LOG_INFO, QString("EVENT_STEREOSCOPIC_STATUS %1").arg(Event.param));
            break;
        default:
            LOG(VB_GENERAL, LOG_INFO, QString("Unknown event %1").arg(Event.event));
      }

    return 0;
}
