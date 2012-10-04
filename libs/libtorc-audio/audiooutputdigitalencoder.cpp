/* Class AudioOutputDigitalEncoder
*
* This file is part of the Torc project.
*
* Based on the class AudioOutputDigitalEncoder from the MythTV project.
* Copyright various authors.
*
* Adapted for Torc by Mark Kendall (2012)
*/

// Qt
#include <QMutex>

// Torc
#include "torccompat.h"
#include "torclocalcontext.h"
#include "torclogging.h"
#include "torcdecoder.h"
#include "torcavutils.h"
#include "audiooutpututil.h"
#include "audiospdifencoder.h"
#include "audiooutputdigitalencoder.h"

// libav headers
extern "C" {
#include "libavutil/mem.h"
#include "libavcodec/avcodec.h"
}

AudioOutputDigitalEncoder::AudioOutputDigitalEncoder(void)
  : m_avContext(NULL),
    m_outBuffer(NULL),
    m_outSize(0),
    m_inBuffer(NULL),
    m_inSize(0),
    m_outLength(0),
    m_inLength(0),
    m_samplesPerFrame(0),
    m_spdifEncoder(NULL)
{
    m_outBuffer = (int16_t *)av_malloc(OUTBUFSIZE);
    if (m_outBuffer)
        m_outSize = OUTBUFSIZE;

    m_inBuffer = (int16_t *)av_malloc(INBUFSIZE);
    if (m_inBuffer)
        m_inSize = INBUFSIZE;
}

AudioOutputDigitalEncoder::~AudioOutputDigitalEncoder()
{
    Dispose();
}

void AudioOutputDigitalEncoder::Dispose()
{
    if (m_avContext)
    {
        avcodec_close(m_avContext);
        av_freep(&m_avContext);
    }

    if (m_outBuffer)
    {
        av_freep(&m_outBuffer);
        m_outSize = 0;
    }

    if (m_inBuffer)
    {
        av_freep(&m_inBuffer);
        m_inSize = 0;
    }

    if (m_spdifEncoder)
        delete m_spdifEncoder;
    m_spdifEncoder = NULL;
}

void *AudioOutputDigitalEncoder::Reallocate(void *Pointer, size_t OldSize, size_t NewSize)
{
    if (!Pointer)
        return NULL;

    // av_realloc doesn't maintain 16 bytes alignment
    void *newpointer = av_malloc(NewSize);
    if (!newpointer)
    {
        av_free(Pointer);
        return newpointer;
    }

    memcpy(newpointer, Pointer, OldSize);
    av_free(Pointer);

    return newpointer;
}

bool AudioOutputDigitalEncoder::Init(CodecID CodecId, int Bitrate, int Samplerate, int Channels)
{
    TorcDecoder::InitialiseLibav();

    LOG(VB_AUDIO, LOG_INFO, QString("Init CodecId: %1 Bitrate: %2 Samplerate: %3 Channels: %4")
        .arg(AVCodecToString(CodecId)).arg(Bitrate).arg(Samplerate).arg(Channels));

    AVCodec* codec = avcodec_find_encoder_by_name("ac3_fixed");
    if (!codec)
    {
        LOG(VB_GENERAL, LOG_ERR, "Could not find codec");
        return false;
    }

    m_avContext = avcodec_alloc_context3(codec);
    avcodec_get_context_defaults3(m_avContext, codec);

    m_avContext->bit_rate    = Bitrate;
    m_avContext->sample_rate = Samplerate;
    m_avContext->channels    = Channels;

    switch (Channels)
    {
        case 1:
            m_avContext->channel_layout = AV_CH_LAYOUT_MONO;
            break;
        case 2:
            m_avContext->channel_layout = AV_CH_LAYOUT_STEREO;
            break;
        case 3:
            m_avContext->channel_layout = AV_CH_LAYOUT_SURROUND;
            break;
        case 4:
            m_avContext->channel_layout = AV_CH_LAYOUT_4POINT0;
            break;
        case 5:
            m_avContext->channel_layout = AV_CH_LAYOUT_5POINT0;
            break;
        default:
            m_avContext->channel_layout = AV_CH_LAYOUT_5POINT1;
            break;
    }

    m_avContext->sample_fmt = AV_SAMPLE_FMT_S16;

    if (avcodec_open2(m_avContext, codec, NULL))
    {
        LOG(VB_GENERAL, LOG_ERR, "Could not open codec, invalid bitrate or samplerate");
        Dispose();
        return false;
    }

    if (m_spdifEncoder)
        delete m_spdifEncoder;

    m_spdifEncoder = new AudioSPDIFEncoder("spdif", CODEC_ID_AC3);
    if (!m_spdifEncoder->Succeeded())
    {
        Dispose();
        LOG(VB_GENERAL, LOG_ERR, "Could not create spdif muxer");
        return false;
    }

    m_samplesPerFrame  = m_avContext->frame_size * m_avContext->channels;

    LOG(VB_AUDIO, LOG_INFO, QString("DigitalEncoder::Init fs=%1, spf=%2")
            .arg(m_avContext->frame_size) .arg(m_samplesPerFrame));

    return true;
}

size_t AudioOutputDigitalEncoder::Encode(void *Buffer, int Length, AudioFormat Format)
{
    // Check if there is enough space in incoming buffer
    int required = m_inLength + Length / AudioOutputSettings::SampleSize(Format) * AudioOutputSettings::SampleSize(FORMAT_S16);

    if (required > (int)m_inSize)
    {
        required = ((required / INBUFSIZE) + 1) * INBUFSIZE;
        LOG(VB_AUDIO, LOG_INFO, QString("low mem, reallocating in buffer from %1 to %2")
            .arg(m_inSize) .arg(required));

        int16_t *tmp = reinterpret_cast<int16_t*>(Reallocate(m_inBuffer, m_inSize, required));
        if (!tmp)
        {
            m_inBuffer = NULL;
            m_inSize = 0;
            return m_outLength;
        }

        m_inBuffer = tmp;
        m_inSize = required;
    }

    if (Format != FORMAT_S16)
    {
        m_inLength += AudioOutputUtil::FromFloat(FORMAT_S16, (char *)m_inBuffer + m_inLength, Buffer, Length);
    }
    else
    {
        memcpy((char*)m_inBuffer + m_inLength, Buffer, Length);
        m_inLength += Length;
    }

    int frames = m_inLength / sizeof(int16_t) / m_samplesPerFrame;
    int i = 0;
    AVFrame *frame = avcodec_alloc_frame();

    while (i < frames)
    {
        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.data          = (uint8_t*)m_encodebuffer;
        pkt.size          = sizeof(m_encodebuffer);
        frame->nb_samples = m_avContext->frame_size;
        frame->data[0]    = (uint8_t *)(m_inBuffer + i * m_samplesPerFrame);
        frame->pts        = AV_NOPTS_VALUE;
        int got_packet    = 0;
        int ret           = avcodec_encode_audio2(m_avContext, &pkt, frame, &got_packet);
        int outsize       = pkt.size;

        if (ret < 0)
        {
            LOG(VB_AUDIO, LOG_ERR, "AC-3 encode error");
            return ret;
        }

        av_free_packet(&pkt);
        i++;
        if (!got_packet)
            continue;

        if (!m_spdifEncoder)
            m_spdifEncoder = new AudioSPDIFEncoder("spdif", CODEC_ID_AC3);

        m_spdifEncoder->WriteFrame((uint8_t *)m_encodebuffer, outsize);
        // Check if output buffer is big enough
        required = m_outLength + m_spdifEncoder->GetProcessedSize();
        if (required > (int)m_outSize)
        {
            required = ((required / OUTBUFSIZE) + 1) * OUTBUFSIZE;
            LOG(VB_AUDIO, LOG_WARNING, QString("low mem, reallocating out buffer from %1 to %2")
                .arg(m_outSize) .arg(required));

            int16_t *tmp = reinterpret_cast<int16_t*>(Reallocate(m_outBuffer, m_outSize, required));
            if (!tmp)
            {
                m_outBuffer = NULL;
                m_outSize = 0;
                return m_outLength;
            }

            m_outBuffer = tmp;
            m_outSize = required;
        }

        int data_size = 0;
        m_spdifEncoder->GetData((uint8_t *)m_outBuffer + m_outLength, data_size);
        m_outLength += data_size;
        m_inLength  -= m_samplesPerFrame * sizeof(int16_t);
    }

    av_free(frame);

    memmove(m_inBuffer, m_inBuffer + i * m_samplesPerFrame, m_inLength);
    return m_outLength;
}

size_t AudioOutputDigitalEncoder::GetFrames(void *Pointer, int MaxLength)
{
    int len = std::min(MaxLength, m_outLength);
    if (len != MaxLength)
        LOG(VB_AUDIO, LOG_INFO, "Getting less than requested");

    memcpy(Pointer, m_outBuffer, len);
    m_outLength -= len;
    memmove(m_outBuffer, (char*)m_outBuffer + len, m_outLength);

    return len;
}

int AudioOutputDigitalEncoder::Buffered(void) const
{
    return m_inLength / sizeof(int16_t) / m_avContext->channels;
}

void AudioOutputDigitalEncoder::Clear(void)
{
    m_inLength = m_outLength = 0;
}
