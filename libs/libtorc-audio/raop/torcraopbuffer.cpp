/* Class TorcRAOPBuffer
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

// Qt
#include <QThread>
#include <QMutex>
#include <QList>
#include <QPair>

// Torc
#include "torccoreutils.h"
#include "torclocalcontext.h"
#include "torclogging.h"
#include "torcavutils.h"
#include "audiodecoder.h"
#include "torcraopdevice.h"
#include "torcraopbuffer.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavformat/url.h"
}

AVInputFormat TorcRAOPDemuxer;

static int ReadRAOPPacket(AVFormatContext *s, AVPacket *pkt)
{
    int ret= av_get_packet(s->pb, pkt, 2048);
    pkt->pts = pkt->dts = AV_NOPTS_VALUE;
    pkt->stream_index = 0;

    if (ret < 0)
        return ret;
    return 0;
}

/*! \class TorcRAOPBuffer
 *  \brief A buffer class for RAOP streams
 *
 * TorcRAOPBuffer is the internal handler for raop:// streams. The
 * supplied URI indicates the unique stream ID and associated audio parameters and
 * data is then requested from the associated TorcRAOPConnection via the
 * singleton TorcRAOPDevice. Incoming data is handled via a custom demuxer,
 * TorcRAOPDemuxer.
 *
 * \sa TorcRAOPDevice
 * \sa TorcRAOPConnection
 *
 * \todo Timestamps
 * \todo Pausing
 * \todo Flushing
 * \todo DACP
*/

TorcRAOPBuffer::TorcRAOPBuffer(void *Parent, const QString &URI, const QUrl &URL, int *Abort)
  : TorcBuffer(Parent, URI, Abort),
    m_url(URL),
    m_streamId(0),
    m_frameSize(-1),
    m_channels(-1),
    m_sampleSize(-1),
    m_historyMult(-1),
    m_initialHistory(-1),
    m_kModifier(-1),
    m_stream(NULL),
    m_avFormatContext(NULL)
{
    static bool initialised = false;

    if (!initialised)
    {
        AudioDecoder::InitialiseLibav();

        initialised = true;
        memset(&TorcRAOPDemuxer, 0, sizeof(AVInputFormat));
        TorcRAOPDemuxer.name = "raop";
        TorcRAOPDemuxer.read_packet = ReadRAOPPacket;
    }
}

TorcRAOPBuffer::~TorcRAOPBuffer()
{
    Close();
}

bool TorcRAOPBuffer::Open(void)
{
    Close();

    // unique stream id
    m_streamId = m_url.port();
    if (m_streamId < 1)
    {
        LOG(VB_GENERAL, LOG_ERR, "Invalid RAOP stream id");
        return false;
    }

    AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_ALAC);

    if (!codec)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to find ALAC codec");
        Close();
        return false;
    }

    m_avFormatContext = avformat_alloc_context();
    m_avFormatContext->iformat = &TorcRAOPDemuxer;
    m_avFormatContext->iformat->flags = AVFMT_NOFILE;
    m_stream = avformat_new_stream(m_avFormatContext, codec);

    if (!m_stream)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to create stream for ALAC");
        Close();
        return false;
    }

    AVRational timebase;
    timebase.num = 1;
    timebase.den = 1;
    m_stream->time_base = timebase;

    if (!m_stream->codec)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to create decoding context for ALAC");
        Close();
        return false;
    }

    QString query = m_url.query();
    QStringList queries = query.split('&');
    foreach (QString pair, queries)
    {
        int index = pair.indexOf('=');
        QString key = pair.left(index);
        QString val = pair.mid(index + 1);

        if (key == "channels")
            m_channels = val.toInt();
        else if (key == "framesize")
            m_frameSize = val.toInt();
        else if (key == "samplesize")
            m_sampleSize = val.toInt();
        else if (key == "historymult")
            m_historyMult = val.toInt();
        else if (key == "initialhistory")
            m_initialHistory = val.toInt();
        else if (key == "kmodifier")
            m_kModifier = val.toInt();
    }

    if (!(m_channels > 0 && m_frameSize > 0 && m_sampleSize > 0 &&
          m_historyMult > -1 && m_initialHistory > -1 && m_kModifier > -1))
    {
        LOG(VB_GENERAL, LOG_ERR, "Invalid stream data");
        Close();
        return false;
    }

    if (m_stream->codec->extradata)
        av_free(m_stream->codec->extradata);

    unsigned char *extradata = new unsigned char[36];
    memset(extradata, 0, 36);
    uint32_t framesize = m_frameSize;
    extradata[12] = (framesize >> 24) & 0xff;
    extradata[13] = (framesize >> 16) & 0xff;
    extradata[14] = (framesize >> 8)  & 0xff;
    extradata[15] = framesize & 0xff;
    extradata[16] = m_channels;
    extradata[17] = m_sampleSize;
    extradata[18] = m_historyMult;
    extradata[19] = m_initialHistory;
    extradata[20] = m_kModifier;

    m_stream->codec->extradata      = extradata;
    m_stream->codec->extradata_size = 36;
    m_stream->codec->channels       = m_channels;
    m_stream->codec->sample_rate    = 44100;
    m_stream->codec->codec_id       = AV_CODEC_ID_ALAC;

    LOG(VB_GENERAL, LOG_INFO, QString("Opened '%1'").arg(m_uri));
    return true;
}

void TorcRAOPBuffer::Close(void)
{
    // reset audio parameters
    m_frameSize      = -1;
    m_channels       = -1;
    m_sampleSize     = -1;
    m_historyMult    = -1;
    m_initialHistory = -1;
    m_kModifier      = -1;

    // close decoder
    if (m_stream && m_stream->codec)
        avcodec_close(m_stream->codec);

    // free decoder context
    if (m_avFormatContext)
        avformat_free_context(m_avFormatContext);

    m_stream = NULL;
    m_avFormatContext = NULL;
}

void* TorcRAOPBuffer::RequiredAVContext(void)
{
    return (void*)m_avFormatContext;
}

int TorcRAOPBuffer::Read(quint8 *Buffer, qint32 BufferSize)
{
    int result = -1;

    QByteArray *data = NULL;
    int tries = 0;

    // need to give TorcRAOPConnection time to recover from missed packets and other temporary interruptions
    while (!(data = TorcRAOPDevice::Read(m_streamId)) && (tries++ < 100) && !m_avFormatContext->interrupt_callback.callback(m_avFormatContext->interrupt_callback.opaque))
        QThread::usleep(50000);

    if (!data)
        return result;

    if (BufferSize != data->size())
        LOG(VB_GENERAL, LOG_WARNING, QString("Packet size %1 - expected %2").arg(data->size()).arg(BufferSize));

    memcpy(Buffer, data->data(), std::min(BufferSize, data->size()));
    result = data->size();
    delete data;
    return result;
}

int TorcRAOPBuffer::Peek(quint8 *Buffer, qint32 BufferSize)
{
    return -1;
}

int TorcRAOPBuffer::Write(quint8 *Buffer, qint32 BufferSize)
{
    return -1;
}

int64_t TorcRAOPBuffer::Seek(int64_t Offset, int Whence)
{
    return -1;
}

qint64 TorcRAOPBuffer::GetSize(void)
{
    return -1;
}

qint64 TorcRAOPBuffer::GetPosition(void)
{
    return -1;
}

bool TorcRAOPBuffer::IsSequential(void)
{
    return true;
}

qint64 TorcRAOPBuffer::BytesAvailable(void)
{
    return 2048;
}

int TorcRAOPBuffer::BestBufferSize(void)
{
    return 2048;
}

bool TorcRAOPBuffer::Pause(void)
{
    return false;
}

bool TorcRAOPBuffer::Unpause(void)
{
    return false;
}

bool TorcRAOPBuffer::TogglePause(void)
{
    return false;
}

class TorcRAOPBufferFactory : TorcBufferFactory
{
    void Score(const QString &URI, const QUrl &URL, int &Score, const bool &Media)
    {
        if (Media && URI.startsWith("raop:", Qt::CaseInsensitive) && Score <= 50)
            Score = 50;
    }

    TorcBuffer* Create(void *Parent, const QString &URI, const QUrl &URL, const int &Score, int *Abort, const bool &Media)
    {
        if (Media && URI.startsWith("raop:", Qt::CaseInsensitive) && Score <= 50)
            return new TorcRAOPBuffer(Parent, URI, URL, Abort);

        return NULL;
    }
} TorcRAOPBufferFactory;
