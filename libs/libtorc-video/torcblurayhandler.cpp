/* Class TorcBlurayHandler
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
#include "torcdirectories.h"
#include "videodecoder.h"
#include "torcbluraymetadata.h"

extern "C" {
#include "libbluray/src/libbluray/bluray-version.h"
#include "libbluray/src/util/log_control.h"
#include "libbluray/src/libbluray/bdnav/bdparse.h"
#include "libbluray/src/libbluray/bdnav/meta_data.h"
}

#include "torcblurayhandler.h"

/*! \class TorcBlurayHandler
 *  \brief Interface between a TorcBuffer (e.g. TorcBlurayBuffer) and libbluray.
 *
 * \todo Check command line playback (i.e. no VideoDecoder...)
 * \todo Detect and use title mode if menu mode not available.
 * \todo ifdef out bd-j support.
 * \todo Cache title/playlist data etc.
*/
TorcBlurayHandler::TorcBlurayHandler(TorcBlurayBuffer *Parent, const QString &Path, int *Abort)
  : m_parent(Parent),
    m_path(Path),
    m_abort(Abort),
    m_bluray(NULL),
    m_currentTitleInfo(NULL),
    m_currentTitleInfoLock(new QMutex(QMutex::Recursive)),
    m_useMenus(false),
    m_allowBDJ(false),
    m_hasBDJ(true)
{
#if defined(CONFIG_BLURAYJAVA) && CONFIG_BLURAYJAVA
    // libbluray looks for jar files in certain hard coded locations. Tell it where our copy is.
    // Will have no effect if bdj support is not available.
    QByteArray location(TORC_LIBBLURAY_JAR_LOCATION);
    qputenv("LIBBLURAY_CP", location);
#endif
}

TorcBlurayHandler::~TorcBlurayHandler()
{
    Close();

    delete m_currentTitleInfoLock;
}

void BlurayLogMessage(const char *Message)
{
    LOG(VB_GENERAL, LOG_INFO, Message);
}

bool TorcBlurayHandler::Open(void)
{
    // setup libbluray logging.
    bd_set_debug_handler(BlurayLogMessage);
    bd_set_debug_mask(DBG_CRIT);

    LOG(VB_GENERAL, LOG_INFO, QString("Using libbluray version: %1").arg(BLURAY_VERSION_STRING));

    // open the disc
    m_bluray = bd_open(m_path.toLocal8Bit().data(), NULL);

    if (!m_bluray)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to open '%1' (Error: '%2'").arg(m_path).arg(strerror(errno)));
        return false;
    }

    // setup the vm
    bd_set_player_setting    (m_bluray, BLURAY_PLAYER_SETTING_PARENTAL,       99);
    bd_set_player_setting_str(m_bluray, BLURAY_PLAYER_SETTING_AUDIO_LANG,    "eng");
    bd_set_player_setting_str(m_bluray, BLURAY_PLAYER_SETTING_PG_LANG,       "eng");
    bd_set_player_setting_str(m_bluray, BLURAY_PLAYER_SETTING_MENU_LANG,     "eng");
    bd_set_player_setting_str(m_bluray, BLURAY_PLAYER_SETTING_COUNTRY_CODE,  "us");
    bd_set_player_setting    (m_bluray, BLURAY_PLAYER_SETTING_REGION_CODE,    1);
    bd_set_player_setting    (m_bluray, BLURAY_PLAYER_SETTING_PLAYER_PROFILE, 0);

    // disc metadata - try libbluray (which will fail with no libxml) and then Torc code.
    TorcBlurayMetadata* bluraymetadata = NULL;
    const meta_dl *metadata = bd_get_meta(m_bluray);
    if (!metadata)
    {
        bluraymetadata = new TorcBlurayMetadata(m_path);
        metadata = bluraymetadata->GetMetadata("eng");
    }

    if (metadata)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Disc Title: %1 (%2)").arg(metadata->di_name).arg(metadata->language_code));
        LOG(VB_GENERAL, LOG_INFO, QString("Alternative Title: %1").arg(metadata->di_alternative));
        LOG(VB_GENERAL, LOG_INFO, QString("Disc Number: %1 of %2").arg(metadata->di_set_number).arg(metadata->di_num_sets));

        if (metadata->toc_count)
            for (int i = 0; i < metadata->toc_count; ++i)
                LOG(VB_GENERAL, LOG_INFO, QString("Title %1: %2").arg(metadata->toc_entries[i].title_number).arg(metadata->toc_entries[i].title_name));

        if (metadata->thumb_count)
            for (int i = 0; i < metadata->thumb_count; ++i)
                LOG(VB_GENERAL, LOG_INFO,QString("Thumbnail: %1x%2 '%3'").arg(metadata->thumbnails[i].xres).arg(metadata->thumbnails[i].yres).arg(metadata->thumbnails[i].path));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to retrieve metadata for '%1'").arg(m_path));
    }

    delete bluraymetadata;

    // disc info
    const BLURAY_DISC_INFO *discinfo = bd_get_disc_info(m_bluray);
    if (discinfo)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("First Play     : %1").arg(discinfo->first_play_supported ? "yes" : "no"));
        LOG(VB_GENERAL, LOG_INFO, QString("Top Menu       : %1").arg(discinfo->top_menu_supported ? "yes" : "no"));
        LOG(VB_GENERAL, LOG_INFO, QString("HDMV Titles    : %1").arg(discinfo->num_hdmv_titles));
        LOG(VB_GENERAL, LOG_INFO, QString("BD-J Titles    : %1").arg(discinfo->num_bdj_titles));
        LOG(VB_GENERAL, LOG_INFO, QString("AACS present   : %1").arg(discinfo->aacs_detected ? "yes" : "no"));
        LOG(VB_GENERAL, LOG_INFO, QString("libaacs used   : %1").arg(discinfo->libaacs_detected ? "yes" : "no"));
        LOG(VB_GENERAL, LOG_INFO, QString("AACS handled   : %1").arg(discinfo->aacs_handled ? "yes" : "no"));
        LOG(VB_GENERAL, LOG_INFO, QString("BD+ present    : %1").arg(discinfo->bdplus_detected ? "yes" : "no"));
        LOG(VB_GENERAL, LOG_INFO, QString("libbdplus used : %1").arg(discinfo->libbdplus_detected ? "yes" : "no"));
        LOG(VB_GENERAL, LOG_INFO, QString("BD+ handled    : %1").arg(discinfo->bdplus_handled ? "yes" : "no"));

        m_hasBDJ = discinfo->num_bdj_titles > 0;
    }

    // retrieve titles and select longest title for 'normal' playback
    int maintitle = 0;

    quint32 titlecount = bd_get_titles(m_bluray, TITLES_RELEVANT, 0);
    quint64 longest = 0;

    for (int i = 0; i < titlecount; i++)
    {
        bd_title_info* titleinfo = bd_get_title_info(m_bluray, i, 0);

        if (titleinfo->duration > longest)
        {
            longest = titleinfo->duration;
            maintitle = i;
        }

        bd_free_title_info(titleinfo);
    }

    // don't use menus if BD-J found and not allowed
    if (m_hasBDJ && !m_allowBDJ)
        m_useMenus = false;

    int error = 0;

    if (m_useMenus)
    {
        // kick the vm
        bd_get_event(m_bluray, NULL);

        // and play
        error = bd_play(m_bluray);
    }
    else
    {
        m_currentTitleInfoLock->lock();
        m_currentTitleInfo = bd_get_title_info(m_bluray, maintitle, 0);
        ProcessTitleInfo();
        m_currentTitleInfoLock->unlock();

        error = bd_select_title(m_bluray, maintitle);
    }

    if (error <= 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to play '%1' (Error: '%2' %3')").arg(m_path).arg(error).arg(strerror(error)));
        return false;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Opened '%1'").arg(m_path));
    return true;
}

void TorcBlurayHandler::Close(void)
{
    // cleanup libbluray provided objects
    if (m_currentTitleInfo)
        bd_free_title_info(m_currentTitleInfo);
    m_currentTitleInfo = NULL;

    // cleanup libbluray
    if (m_bluray)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Closing '%1'").arg(m_path));
        bd_close(m_bluray);
        m_bluray = NULL;
    }
}

int TorcBlurayHandler::Read(quint8 *Buffer, qint32 BufferSize)
{
    if (m_bluray)
        return bd_read(m_bluray, (unsigned char *)Buffer, BufferSize);

    return AVERROR_UNKNOWN;
}

QString TimeToString(quint64 Time)
{
    uint64_t totalseconds = Time / 90000;
    int hours   = (int)totalseconds / 60 / 60;
    int minutes = ((int)totalseconds / 60) - (hours * 60);
    double secs = (double)totalseconds - (double)(hours * 60 * 60 + minutes * 60);
    return QString("%1:%2:%3").arg(QString().sprintf("%02d", hours)).arg(QString().sprintf("%02d", minutes)).arg(QString().sprintf("%02.1f", secs));
}

void TorcBlurayHandler::ProcessTitleInfo(void)
{
    QMutexLocker locker(m_currentTitleInfoLock);

    if (!m_currentTitleInfo)
        return;

    // general information
    LOG(VB_GENERAL, LOG_INFO, QString("New title: Index %1 Playlist %2 Chapters %3 Angles %4")
        .arg(m_currentTitleInfo->idx).arg(m_currentTitleInfo->playlist).arg(m_currentTitleInfo->chapter_count).arg(m_currentTitleInfo->angle_count));

    LOG(VB_GENERAL, LOG_INFO, QString("New title: Clips %1 Marks %2 Duration %3")
        .arg(m_currentTitleInfo->clip_count).arg(m_currentTitleInfo->mark_count).arg(TimeToString(m_currentTitleInfo->duration)));

    /*
    // Clips
    for (int i = 0; i < m_currentTitleInfo->clip_count; ++i)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Clip #%1: Stillmode %2 Stilltime %3 VideoStreams %4 AudioStreams %5 PGStreams %6 IGStreams %7")
            .arg(i + 1).arg(m_currentTitleInfo->clips[i].still_mode).arg(m_currentTitleInfo->clips[i].still_time).arg(m_currentTitleInfo->clips[i].video_stream_count)
            .arg(m_currentTitleInfo->clips[i].audio_stream_count).arg(m_currentTitleInfo->clips[i].pg_stream_count).arg(m_currentTitleInfo->clips[i].ig_stream_count));
        LOG(VB_GENERAL, LOG_INFO, QString("Clip #%1: SecondaryVideo %2 SecondaryAudio %3 Starttime %4 InTime %5 OutTime %6")
            .arg(i + 1).arg(m_currentTitleInfo->clips[i].sec_video_stream_count).arg(m_currentTitleInfo->clips[i].sec_audio_stream_count)
            .arg(TimeToString(m_currentTitleInfo->clips[i].start_time)).arg(TimeToString(m_currentTitleInfo->clips[i].in_time))
            .arg(TimeToString(m_currentTitleInfo->clips[i].out_time)));
    }
    */
    // Chapters
    for (int i = 0; i < m_currentTitleInfo->chapter_count; ++i)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Chapter #%1: Index %2 Start %3 Duration %4 Offset %5 Clipref %6")
            .arg(i + 1).arg(m_currentTitleInfo->chapters[i].idx).arg(TimeToString(m_currentTitleInfo->chapters[i].start))
            .arg(TimeToString(m_currentTitleInfo->chapters[i].duration)).arg(m_currentTitleInfo->chapters[i].offset).arg(m_currentTitleInfo->chapters[i].clip_ref));
    }

    /*
    // Marks
    for (int i = 0; i < m_currentTitleInfo->mark_count; ++i)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Mark #%1: Index %2 Start %3 Duration %4 Offset %5 Clipref %6")
            .arg(i + 1).arg(m_currentTitleInfo->marks[i].idx).arg(TimeToString(m_currentTitleInfo->marks[i].start))
            .arg(TimeToString(m_currentTitleInfo->marks[i].duration)).arg(m_currentTitleInfo->marks[i].offset).arg(m_currentTitleInfo->marks[i].clip_ref));
    }
    */
}

qreal TorcBlurayHandler::GetBlurayFramerate(quint8 Rate)
{
    switch (Rate)
    {
        case BD_VIDEO_RATE_24000_1001:
            return 23.97;
            break;
        case BD_VIDEO_RATE_24:
            return 24;
            break;
        case BD_VIDEO_RATE_25:
            return 25;
            break;
        case BD_VIDEO_RATE_30000_1001:
            return 29.97;
            break;
        case BD_VIDEO_RATE_50:
            return 50;
            break;
        case BD_VIDEO_RATE_60000_1001:
            return 59.94;
            break;
        default:
            return 0;
            break;
    }

    return 0.0;
}
