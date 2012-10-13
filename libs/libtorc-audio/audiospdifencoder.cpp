/* Class AudioSPDIFEncoder
*
* This file is part of the Torc project.
*
* Based on the class SPDIFEncoder from the MythTV project.
* Copyright various authors.
*
* Adapted for Torc by Mark Kendall (2012)
*/

// Qt
#include <QMutex>

// Torc
#include "torclocalcontext.h"
#include "torclogging.h"
#include "torccompat.h"
#include "torcavutils.h"
#include "audiodecoder.h"
#include "audiospdifencoder.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libavutil/opt.h"
}

/**
 * AudioSPDIFEncoder constructor
 * Args:
 *   QString muxer       : name of the muxer.
 *                         Use "spdif" for IEC 958 or IEC 61937 encapsulation
 *                         (AC3, DTS, E-AC3, TrueHD, DTS-HD-MA)
 *                         Use "adts" for ADTS encpsulation (AAC)
 *   AVCodecContext *ctx : CodecContext to be encaspulated
 */
AudioSPDIFEncoder::AudioSPDIFEncoder(QString Muxer, int CodecId)
  : m_complete(false),
    m_formatContext(NULL),
    m_stream(NULL),
    m_size(0)
{
    AudioDecoder::InitialiseLibav();

    QByteArray dev_ba = Muxer.toAscii();
    AVOutputFormat *fmt = av_guess_format(dev_ba.constData(), NULL, NULL);
    if (!fmt)
    {
        LOG(VB_AUDIO, LOG_ERR, "av_guess_format");
        return;
    }

    m_formatContext = avformat_alloc_context();
    if (!m_formatContext)
    {
        LOG(VB_AUDIO, LOG_ERR, "avformat_alloc_context");
        return;
    }

    m_formatContext->oformat = fmt;
    m_formatContext->pb = avio_alloc_context(m_buffer, sizeof(m_buffer), 0,
                                  this, NULL, EncoderCallback, NULL);
    if (!m_formatContext->pb)
    {
        LOG(VB_AUDIO, LOG_ERR, "avio_alloc_context");
        Destroy();
        return;
    }

    m_formatContext->pb->seekable = 0;
    m_formatContext->flags       |= AVFMT_NOFILE | AVFMT_FLAG_IGNIDX;

    m_stream = avformat_new_stream(m_formatContext, NULL);
    if (!m_stream)
    {
        LOG(VB_AUDIO, LOG_ERR, "avformat_new_stream");
        Destroy();
        return;
    }

    m_stream->id          = 1;
    AVCodecContext *codec = m_stream->codec;
    codec->codec_id       = (CodecID)CodecId;
    avformat_write_header(m_formatContext, NULL);

    LOG(VB_AUDIO, LOG_INFO, QString("Creating '%1' encoder (for %2)").arg(Muxer).arg(AVCodecToString(CodecId)));

    m_complete = true;
}

AudioSPDIFEncoder::~AudioSPDIFEncoder(void)
{
    Destroy();
}

void AudioSPDIFEncoder::WriteFrame(unsigned char *Data, int Size)
{
    AVPacket packet;
    av_init_packet(&packet);
    static int pts = 1; // to avoid warning "Encoder did not produce proper pts"
    packet.pts  = pts++; 
    packet.data = Data;
    packet.size = Size;

    if (av_write_frame(m_formatContext, &packet) < 0)
        LOG(VB_AUDIO, LOG_ERR, "av_write_frame");
}

/**
 * Retrieve encoded data and copy it in the provided buffer.
 * Return -1 if there is no data to retrieve.
 * On return, dest_size will contain the length of the data copied
 * Upon completion, the internal encoder buffer is emptied.
 */
int AudioSPDIFEncoder::GetData(unsigned char *Buffer, int &Size)
{
    if(m_size > 0)
    {
        memcpy(Buffer, m_buffer, m_size);
        Size = m_size;
        m_size = 0;
        return Size;
    }

    return -1;
}

int AudioSPDIFEncoder::GetProcessedSize(void)
{
    return m_size;
}

unsigned char* AudioSPDIFEncoder::GetProcessedBuffer(void)
{
    return m_buffer;
}

/**
 * Reset the internal encoder buffer
 */
void AudioSPDIFEncoder::Reset(void)
{
    m_size = 0;
}

bool AudioSPDIFEncoder::Succeeded(void)
{
    return m_complete;
}

/**
 * Set the maximum HD rate.
 * If playing DTS-HD content, setting a HD rate of 0 will only use the DTS-Core
 * and the HD stream be stripped out before encoding
 * Input: rate = maximum HD rate in Hz
 */
bool AudioSPDIFEncoder::SetMaxHDRate(int Rate)
{
    if (!m_formatContext)
        return false;

    av_opt_set_int(m_formatContext->priv_data, "dtshd_rate", Rate, 0);
    return true;
}

/**
 * EncoderCallback: Internal callback function that will receive encoded frames
 */
int AudioSPDIFEncoder::EncoderCallback(void *Object, unsigned char *Buffer, int Size)
{
    AudioSPDIFEncoder *enc = (AudioSPDIFEncoder*)Object;

    memcpy(enc->m_buffer + enc->m_size, Buffer, Size);
    enc->m_size += Size;
    return Size;
}

void AudioSPDIFEncoder::Destroy(void)
{
    Reset();

    if (m_complete)
        av_write_trailer(m_formatContext);

    if (m_stream)
    {
        delete[] m_stream->codec->extradata;
        avcodec_close(m_stream->codec);
        av_freep(&m_stream);
    }

    if (m_formatContext)
    {
        if (m_formatContext->pb)
            av_freep(&m_formatContext->pb);
        av_freep(&m_formatContext);
    }
}
