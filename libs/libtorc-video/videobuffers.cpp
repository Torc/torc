/* Class VideoBuffers
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012
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
#include "torcconfig.h"
#include "torclogging.h"
#include "videoframe.h"
#include "videobuffers.h"

#define MAX_WAIT_FOR_UNUSED_FRAME 10000000
#define MAX_WAIT_FOR_READY_FRAME  1000000

// roughly 1/120 or 1/2 frame interval at 60fps
#define WAIT_FOR_UNUSED_RETRY 8000
// roughly 1/240 or 1/4 frame interval at 60fps
#define WAIT_FOR_READY_RETRY  4000

#define MIN_BUFFERS_FOR_DECODE  2
#define MIN_BUFFERS_FOR_DISPLAY 6

extern "C" {
#include "libavutil/pixdesc.h"
}

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

/*! \class VideoBuffers
 *  \brief A class to track and manage video buffers.
 *
 * VideoBuffers maintains a set of queues that track the state of individual
 * VideoFrames.
 *
 * A VideoFrame can exist in only one of these queues, with the exception of
 * Decoded, which tracks those frames still in use in any way by the decoder.
 *
 *             Decoder            Display            Deinterlacing
 * Unused      -                  -                  -
 * Decoding    decoding           not ready          -
 * Ready       decoded            ready              -
 * Displaying  decoded,           displaying         -
 * Displayed   decoded,           displayed          in use reference
 * Reference   in use reference   displayed          -
 *
 * Decoded     decoded            ready              -
 */

VideoBuffers::VideoBuffers()
  : m_frameCount(0),
    m_referenceFrames(MIN_BUFFERS_FOR_DECODE),
    m_displayFrames(MIN_BUFFERS_FOR_DISPLAY),
    m_lock(new QMutex(QMutex::Recursive)),
    m_currentFormat(PIX_FMT_NONE),
    m_currentWidth(0),
    m_currentHeight(0),
    m_preferredDisplayFormat(PIX_FMT_YUV420P)
{
}

VideoBuffers::~VideoBuffers()
{
    Reset(true);
    delete m_lock;
}

void VideoBuffers::Debug(void)
{
    m_lock->lock();
    int unused     = m_unused.size();
    int decoding   = m_decoding.size();
    int decoded    = m_decoded.size();
    int ready      = m_ready.size();
    int displaying = m_displaying.size();
    int displayed  = m_displayed.size();
    int reference  = m_reference.size();
    int total = unused + decoding + ready + displaying + reference;
    m_lock->unlock();

    LOG(VB_GENERAL, LOG_INFO, QString("Frames Total: %1(%2) U%3 D%4 R%5 D%6 D%7 R%8 (D%9)")
        .arg(total).arg(m_frameCount).arg(unused).arg(decoding).arg(ready)
        .arg(displaying).arg(displayed).arg(reference).arg(decoded));
}

void VideoBuffers::FormatChanged(PixelFormat Format, int Width, int Height, int ReferenceFrames)
{
    // TODO this could be extended
    // First - allow for small differences within macroblock bounds
    // Second - retain buffers when new resolution is smaller

    QMutexLocker locker(m_lock);

    bool referenceschanged = ReferenceFrames != m_referenceFrames && ReferenceFrames >= MIN_BUFFERS_FOR_DECODE;
    bool formatchanged     = Format != m_currentFormat || Width != m_currentWidth || Height != m_currentHeight;

    // nothing to do
    if (!formatchanged && !referenceschanged)
        return;

    if (formatchanged)
        SetFormat(Format, Width, Height);

    // update minimum reserved for decoder
    if (referenceschanged)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Reserving %1 frames for decoder").arg(ReferenceFrames));
        m_referenceFrames = ReferenceFrames;
    }

    // delete everything in unused and mark everything else as discards...
    if (formatchanged)
    {
        // TODO how to deal with decoded here?

        while (!m_unused.isEmpty())
        {
            delete m_unused.takeFirst();
            m_frameCount--;
        }

        foreach (VideoFrame *frame, m_decoding)
            frame->SetDiscard();
        foreach (VideoFrame *frame, m_ready)
            frame->SetDiscard();
        foreach (VideoFrame *frame, m_displaying)
            frame->SetDiscard();
        foreach (VideoFrame *frame, m_displayed)
            frame->SetDiscard();
        foreach (VideoFrame *frame, m_reference)
            frame->SetDiscard();
    }

    // if the number of reference frames is lower, recover the excess
    if (referenceschanged && !formatchanged)
    {
        int framestorecover = m_frameCount - (m_referenceFrames + m_displayFrames);
        while (framestorecover > 0)
        {
            QList<VideoFrame*>::iterator it;
            if (m_reference.size())
                for (it = m_reference.begin() ; it != m_reference.end(), framestorecover > 0; ++it, --framestorecover)
                    (*it)->SetDiscard();
            else if (m_displayed.size())
                for (it = m_displayed.begin() ; it != m_displayed.end(), framestorecover > 0; ++it, --framestorecover)
                    (*it)->SetDiscard();
            else if (m_displaying.size())
                for (it = m_displaying.begin() ; it != m_displaying.end(), framestorecover > 0; ++it, --framestorecover)
                    (*it)->SetDiscard();
            else if (m_ready.size())
                for (it = m_ready.begin() ; it != m_ready.end(), framestorecover > 0; ++it, --framestorecover)
                    (*it)->SetDiscard();
            else if (m_unused.size())
                for (it = m_unused.begin() ; it != m_unused.end(), framestorecover > 0; ++it, --framestorecover)
                    (*it)->SetDiscard();
            else
                break;
        }
    }

    CheckDecodedFrames();
}

void VideoBuffers::SetFormat(PixelFormat Format, int Width, int Height)
{
    LOG(VB_GENERAL, LOG_INFO, QString("Video buffer format: %1 %2x%3")
        .arg(av_get_pix_fmt_name(Format)).arg(Width).arg(Height));

    m_currentFormat = Format;
    m_currentWidth  = Width;
    m_currentHeight = Height;
}

void VideoBuffers::SetDisplayFormat(PixelFormat Format)
{
    m_preferredDisplayFormat = Format;
}

void VideoBuffers::Reset(bool DeleteFrames)
{
    QMutexLocker locker(m_lock);

    // move all frames into unused
    while (!m_decoding.isEmpty())
        m_unused.append(m_decoding.takeFirst());
    while (!m_ready.isEmpty())
        m_unused.append(m_ready.takeFirst());
    while (!m_displaying.isEmpty())
        m_unused.append(m_displaying.takeFirst());
    while (!m_displayed.isEmpty())
        m_unused.append(m_displayed.takeFirst());
    while (!m_reference.isEmpty())
        m_unused.append(m_reference.takeFirst());
    m_decoded.clear();

    while (DeleteFrames && !m_unused.isEmpty())
    {
        delete m_unused.takeFirst();
        m_frameCount--;
    }
}

bool VideoBuffers::GetBufferStatus(int &Unused, int &Inuse, int &Held)
{
    if (m_lock->tryLock())
    {
        Unused = m_unused.size();
        int notcreated = (m_referenceFrames + m_displayFrames) - m_frameCount;
        if (notcreated > 0)
            Unused += notcreated;

        Inuse = m_decoding.size() + m_displaying.size();
        Held  = m_displayed.size() + m_reference.size();

        m_lock->unlock();
        return true;
    }

    return false;
}

VideoFrame* VideoBuffers::GetFrameForDecoding(void)
{
    VideoFrame *frame = NULL;

    {
        QMutexLocker locker(m_lock);

        // create new frame if still below limit
        if (m_frameCount < (m_referenceFrames + m_displayFrames))
        {
            frame = new VideoFrame(m_preferredDisplayFormat);
            frame->Initialise(m_currentFormat, m_currentWidth, m_currentHeight);
            m_frameCount++;
            m_decoding.append(frame);
            return frame;
        }

        // return unused frame if available
        if (!m_unused.isEmpty())
        {
            frame = m_unused.takeFirst();
            m_decoding.append(frame);
            return frame;
        }
    }

    // wait for unused frame
    int tries = MAX_WAIT_FOR_UNUSED_FRAME / WAIT_FOR_UNUSED_RETRY;

    while (tries-- >= 0)
    {
        m_lock->lock();

        if (m_unused.isEmpty())
        {
            m_lock->unlock();
            usleep(WAIT_FOR_UNUSED_RETRY);
            continue;
        }

        frame = m_unused.takeFirst();
        m_decoding.append(frame);
        m_lock->unlock();
    }

    return frame;
}

void VideoBuffers::ReleaseFrameFromDecoding(VideoFrame *Frame)
{
    if (!Frame)
        return;

    QMutexLocker locker(m_lock);

    if (m_decoding.contains(Frame))
        m_decoding.removeOne(Frame);
    else
        LOG(VB_GENERAL, LOG_ERR, "Decoder releasing unknown frame");

    m_decoded.append(Frame);
    m_ready.append(Frame);
}

void VideoBuffers::ReleaseFrameFromDecoded(VideoFrame *Frame)
{
    if (!Frame)
        return;

    QMutexLocker locker(m_lock);

    if (m_decoded.contains(Frame))
    {
        m_decoded.removeOne(Frame);
    }
    else if (m_decoding.contains(Frame))
    {
        m_decoding.removeOne(Frame);
        m_unused.append(Frame);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Decoder releasing unknown frame");
    }
}

VideoFrame* VideoBuffers::GetFrameForDisplaying(int WaitUSecs /*= 0*/)
{
    VideoFrame *frame = NULL;


    int tries = (std::min(WaitUSecs, MAX_WAIT_FOR_READY_FRAME)) / WAIT_FOR_READY_RETRY;

    while (tries-- >= 0)
    {
        m_lock->lock();

        if (m_ready.isEmpty())
        {
            m_lock->unlock();
            usleep(WAIT_FOR_READY_RETRY);
            continue;
        }

        frame = m_ready.takeFirst();
        m_displaying.append(frame);
        m_lock->unlock();
    }

    return frame;
}

void VideoBuffers::ReleaseFrameFromDisplaying(VideoFrame *Frame, bool InUseForDeinterlacer)
{
    if (!Frame)
        return;

    QMutexLocker locker(m_lock);

    if (m_displaying.contains(Frame))
        m_displaying.removeOne(Frame);
    else
        LOG(VB_GENERAL, LOG_ERR, "Display releasing unknown frame");

    if (InUseForDeinterlacer)
    {
        m_displayed.append(Frame);
    }
    else
    {
        m_reference.append(Frame);
        CheckDecodedFrames();
    }
}

void VideoBuffers::ReleaseFrameFromDisplayed(VideoFrame *Frame)
{
    if (!Frame)
        return;

    QMutexLocker locker(m_lock);

    if (m_displayed.contains(Frame))
        m_displayed.removeOne(Frame);
    else
        LOG(VB_GENERAL, LOG_ERR, "Display releasing unknown frame");

    m_reference.append(Frame);
    CheckDecodedFrames();
}

void VideoBuffers::CheckDecodedFrames(void)
{
    QMutexLocker locker(m_lock);

    QList<VideoFrame*> recovered;
    QList<VideoFrame*>::iterator it = m_reference.begin();
    for ( ; it != m_reference.end(); ++it)
        if (!m_decoded.contains((*it)))
            recovered.append((*it));

    while (!recovered.isEmpty())
    {
        VideoFrame* frame = recovered.takeFirst();
        m_reference.removeOne(frame);
        if (frame->Discard())
        {
            delete frame;
            m_frameCount--;
        }
        else
        {
            m_unused.append(frame);
        }
    }
}
