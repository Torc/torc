/* Class AudioDecoder
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
#include <QMutex>
#include <QLinkedList>
#include <QWaitCondition>

// Torc
#include "torccoreutils.h"
#include "torclocalcontext.h"
#include "torclanguage.h"
#include "torclogging.h"
#include "torcqthread.h"
#include "torctimer.h"
#include "torcbuffer.h"
#include "torcavutils.h"
#include "audiooutputsettings.h"
#include "audiowrapper.h"
#include "audiodecoder.h"

extern "C" {
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libavutil/pixdesc.h"
#include "libavutil/opt.h"
#include "libavresample/avresample.h"
}

#define PROBE_BUFFER_SIZE (512 * 1024)
#define MAX_QUEUE_SIZE_AUDIO   (20 * 16 * 1024)
#define MAX_QUEUE_SIZE_VIDEO   (20 * 1024 * 1024)
#define MAX_QUEUE_LENGTH_AUDIO 100

class TorcChapter
{
  public:
    TorcChapter()
      : m_id(0),
        m_startTime(0)
    {
    }

    int                   m_id;
    qint64                m_startTime;
    QMap<QString,QString> m_avMetaData;
};

class TorcStreamData
{
  public:
    TorcStreamData()
      : m_type(StreamTypeUnknown),
        m_index(-1),
        m_id(-1),
        m_codecID(AV_CODEC_ID_NONE),
        m_secondaryIndex(-1),
        m_avDisposition(AV_DISPOSITION_DEFAULT),
        m_language(DEFAULT_QT_LANGUAGE),
        m_originalChannels(0),
        m_width(0),
        m_height(0)
    {
    }

    bool IsValid(void)
    {
        return (m_type > StreamTypeUnknown) && (m_type < StreamTypeEnd) && (m_index > -1);
    }

    TorcStreamTypes       m_type;
    int                   m_index;
    int                   m_id;
    AVCodecID             m_codecID;
    int                   m_secondaryIndex;
    int                   m_avDisposition;
    QLocale::Language     m_language;
    int                   m_originalChannels;
    int                   m_width;
    int                   m_height;
    QMap<QString,QString> m_avMetaData;
    QByteArray            m_attachment;
    QByteArray            m_subtitleHeader;
};

class TorcProgramData
{
  public:
    TorcProgramData()
      : m_id(0),
        m_index(0),
        m_streamCount(0)
    {
    }

    ~TorcProgramData()
    {
        for (int i = 0; i < StreamTypeEnd; ++i)
            while (!m_streams[i].isEmpty())
                delete m_streams[i].takeLast();
    }

    bool IsValid(void)
    {
        return m_streamCount > 0;
    }


    int                    m_id;
    uint                   m_index;
    QMap<QString,QString>  m_avMetaData;
    QList<TorcStreamData*> m_streams[StreamTypeEnd];
    int                    m_streamCount;
};

QString AudioDecoder::StreamTypeToString(TorcStreamTypes Type)
{
    switch (Type)
    {
        case StreamTypeAudio:      return QString("Audio");
        case StreamTypeVideo:      return QString("Video");
        case StreamTypeSubtitle:   return QString("Subtitle");
        case StreamTypeAttachment: return QString("Attachment");
        default: break;
    }

    return QString("Unknown");
}

int AudioDecoder::DecoderInterrupt(void *Object)
{
    int* abort = (int*)Object;
    if (abort && *abort > 0)
    {
        LOG(VB_GENERAL, LOG_INFO, "Aborting decoder");
        return 1;
    }

    return 0;
}

static int TorcAVLockCallback(void **Mutex, enum AVLockOp Operation)
{
    (void)Mutex;

    if (AV_LOCK_OBTAIN == Operation)
        gAVCodecLock->lock();
    else if (AV_LOCK_RELEASE == Operation)
        gAVCodecLock->unlock();

    return 0;
}

static void TorcAVLogCallback(void* Object, int Level, const char* Format, va_list List)
{
    uint64_t mask  = VB_GENERAL;
    LogLevel level = LOG_DEBUG;

    switch (Level)
    {
        case AV_LOG_PANIC:
            level = LOG_EMERG;
            break;
        case AV_LOG_FATAL:
            level = LOG_CRIT;
            break;
        case AV_LOG_ERROR:
            level = LOG_ERR;
            mask |= VB_LIBAV;
            break;
        case AV_LOG_DEBUG:
            level = LOG_DEBUG;
            mask |= VB_LIBAV;
            break;
        case AV_LOG_WARNING:
            mask |= VB_LIBAV;
            break;
        case AV_LOG_VERBOSE:
        case AV_LOG_INFO:
            level = LOG_INFO;
            break;
        default:
            return;
    }

    if (!VERBOSE_LEVEL_CHECK(mask, level))
        return;

    QString header;
    if (Object)
    {
        AVClass* avclass = *(AVClass**)Object;
        header.sprintf("[%s@%p] ", avclass->item_name(Object), avclass);
    }

    static const int length = 255;

    char message[length + 1];
    int used = vsnprintf(message, length + 1, Format, List);

    if (used > length)
    {
        message[length - 3] = '.';
        message[length - 2] = '.';
        message[length - 1] = '\n';
    }

    LOG(mask, level, header + QString::fromLocal8Bit(message).trimmed());
}

static AVPacket gFlushCodec;

class TorcPacketQueue
{
  public:
    TorcPacketQueue()
      : m_length(0),
        m_size(0),
        m_lock(new QMutex()),
        m_wait(new QWaitCondition())
    {
    }

    ~TorcPacketQueue()
    {
        Flush(false, false);

        delete m_lock;
        delete m_wait;
    }

    void Flush(bool FlushQueue, bool FlushCodec)
    {
        m_lock->lock();

        if (FlushQueue)
        {
            while (!m_queue.isEmpty())
            {
                AVPacket* packet = Pop();
                if (packet != &gFlushCodec)
                {
                    av_free_packet(packet);
                    delete packet;
                }
            }
        }

        if (FlushCodec)
        {
            m_queue.append(&gFlushCodec);
            m_size += (sizeof(&gFlushCodec) + gFlushCodec.size);
            m_length++;
        }

        m_lock->unlock();

        if (FlushCodec)
            m_wait->wakeAll();
    }

    qint64 Size(void)
    {
        return m_size;
    }

    int Length(void)
    {
        return m_length;
    }

    AVPacket* Pop(void)
    {
        if (m_queue.isEmpty())
            return NULL;

        AVPacket* packet = m_queue.takeFirst();
        m_size -= (sizeof(packet) + packet->size);
        m_length--;
        return packet;
    }

    bool Push(AVPacket* &Packet)
    {
        m_lock->lock();
        (void)av_dup_packet(Packet);
        m_queue.append(Packet);
        m_size += (sizeof(Packet) + Packet->size);
        m_length++;
        m_lock->unlock();
        m_wait->wakeAll();
        Packet = NULL;
        return true;
    }

    int                    m_length;
    qint64                 m_size;
    QMutex                *m_lock;
    QWaitCondition        *m_wait;
    QLinkedList<AVPacket*> m_queue;
};

class TorcDecoderThread : public TorcQThread
{
  public:
    TorcDecoderThread(AudioDecoder* Parent, const QString &Name, bool Queue = true)
      : TorcQThread(Name),
        m_parent(Parent),
        m_queue(Queue ? new TorcPacketQueue() : NULL),
        m_threadRunning(false),
        m_state(TorcDecoder::None),
        m_requestedState(TorcDecoder::None),
        m_demuxerState(TorcDecoder::DemuxerReady),
        m_internalBufferEmpty(true)
    {
    }

    virtual ~TorcDecoderThread()
    {
        delete m_queue;
    }

    bool IsRunning(void)
    {
        return m_threadRunning;
    }

    bool IsPaused(void)
    {
        return m_state == TorcDecoder::Paused;
    }

    void Stop(void)
    {
        m_requestedState = TorcDecoder::Stopped;
        if (m_queue)
            m_queue->m_wait->wakeAll();
    }

    void Pause(void)
    {
        m_requestedState = TorcDecoder::Paused;
        if (m_queue)
            m_queue->m_wait->wakeAll();
    }

    void Unpause(void)
    {
        m_requestedState = TorcDecoder::Running;
        if (m_queue)
            m_queue->m_wait->wakeAll();
    }

    bool Wait(int MSecs = 0)
    {
        TorcTimer timer;
        if (MSecs)
            timer.Start();

        while (m_threadRunning && (!MSecs || (MSecs && (timer.Elapsed() <= MSecs))))
            QThread::usleep(50000);

        if (m_threadRunning)
        {
            LOG(VB_GENERAL, LOG_WARNING, QString("Thread '%1' failed to stop").arg(objectName()));
            return false;
        }

        return true;
    }

    void run(void)
    {
        Initialise();
        m_threadRunning = true;
        RunFunction();
        m_threadRunning = false;
        Deinitialise();
    }

    void Start(void)
    {
    }

    void Finish(void)
    {
    }

    virtual void RunFunction(void) = 0;

    AudioDecoder              *m_parent;
    TorcPacketQueue           *m_queue;
    bool                       m_threadRunning;
    TorcDecoder::DecoderState  m_state;
    TorcDecoder::DecoderState  m_requestedState;
    TorcDecoder::DemuxerState  m_demuxerState;
    bool                       m_internalBufferEmpty;
};

class TorcVideoThread : public TorcDecoderThread
{
  public:
    TorcVideoThread(AudioDecoder* Parent)
      : TorcDecoderThread(Parent, "VideoDecode")
    {
    }

    void RunFunction(void)
    {
        LOG(VB_GENERAL, LOG_INFO, "Video thread starting");

        if (m_parent)
            m_parent->DecodeVideoFrames(this);

        LOG(VB_GENERAL, LOG_INFO, "Video thread stopping");
    }
};

class TorcAudioThread : public TorcDecoderThread
{
  public:
    TorcAudioThread(AudioDecoder* Parent)
      : TorcDecoderThread(Parent, "AudioDecode")
    {
    }

    void RunFunction(void)
    {
        LOG(VB_GENERAL, LOG_INFO, "Audio thread starting");

        if (m_parent)
            m_parent->DecodeAudioFrames(this);

        LOG(VB_GENERAL, LOG_INFO, "Audio thread stopping");
    }
};

class TorcSubtitleThread : public TorcDecoderThread
{
  public:
    TorcSubtitleThread(AudioDecoder* Parent)
      : TorcDecoderThread(Parent, "SubsDecode")
    {
    }

    void RunFunction(void)
    {
        LOG(VB_GENERAL, LOG_INFO, "Subtitle thread starting");

        if (m_parent)
            m_parent->DecodeSubtitles(this);

        LOG(VB_GENERAL, LOG_INFO, "Subtitle thread stopping");
    }
};

class TorcDemuxerThread : public TorcDecoderThread
{
  public:
    TorcDemuxerThread(AudioDecoder* Parent)
      : TorcDecoderThread(Parent, "Demuxer", false),
        m_videoThread(new TorcVideoThread(Parent)),
        m_audioThread(new TorcAudioThread(Parent)),
        m_subtitleThread(new TorcSubtitleThread(Parent))
    {
    }

    ~TorcDemuxerThread()
    {
        delete m_videoThread;
        delete m_audioThread;
        delete m_subtitleThread;
    }

    void RunFunction(void)
    {
        LOG(VB_GENERAL, LOG_INFO, "Demuxer thread starting");

        if (m_parent)
            if (m_parent->OpenDemuxer(this))
                m_parent->DemuxPackets(this);

        LOG(VB_GENERAL, LOG_INFO, "Demuxer thread stopping");
    }

    TorcVideoThread    *m_videoThread;
    TorcAudioThread    *m_audioThread;
    TorcSubtitleThread *m_subtitleThread;
};

class AudioDecoderPriv
{
  public:
    AudioDecoderPriv(AudioDecoder *Parent)
      : m_buffer(NULL),
        m_libavBuffer(NULL),
        m_libavBufferSize(0),
        m_avFormatContext(NULL),
        m_createdAVFormatContext(false),
        m_pauseResult(0),
        m_demuxerThread(new TorcDemuxerThread(Parent)),
        m_audioResampleContext(NULL),
        m_audioResampleChannelLayout(0),
        m_audioResampleFormat(AV_SAMPLE_FMT_NONE),
        m_audioResampleSampleRate(0)
    {
    }

    ~AudioDecoderPriv()
    {
        if (m_audioResampleContext)
            avresample_free(&m_audioResampleContext);
        delete m_demuxerThread;
    }

    int DecodeAudioPacket  (AVCodecContext* Context, quint8 *Buffer, int &DataSize, AVPacket *Packet);

    TorcBuffer             *m_buffer;
    unsigned char          *m_libavBuffer;
    int                     m_libavBufferSize;
    AVFormatContext        *m_avFormatContext;
    bool                    m_createdAVFormatContext;
    int                     m_pauseResult;
    TorcDemuxerThread      *m_demuxerThread;

    AVAudioResampleContext *m_audioResampleContext;
    uint64_t                m_audioResampleChannelLayout;
    AVSampleFormat          m_audioResampleFormat;
    int                     m_audioResampleSampleRate;
};

void AudioDecoder::InitialiseLibav(void)
{
    static bool initialised = false;

    if (initialised)
        return;

    initialised = true;

    // Packet queue flush signal
    static QByteArray flush("flush");
    av_init_packet(&gFlushCodec);
    gFlushCodec.data = (uint8_t*)flush.data();

    // Libav logging
    av_log_set_level(VERBOSE_LEVEL_CHECK(VB_LIBAV, LOG_ANY) ? AV_LOG_DEBUG : AV_LOG_ERROR);
    av_log_set_callback(&TorcAVLogCallback);

    if (av_lockmgr_register(&TorcAVLockCallback) < 0)
        LOG(VB_GENERAL, LOG_ERR, "Failed to register global libav lock function");

    {
        QMutexLocker locker(gAVCodecLock);
        av_register_all();
        avformat_network_init();
        avdevice_register_all();
    }

    LOG(VB_GENERAL, LOG_INFO, "Libav initialised");
}

/*! \class AudioDecoder
 *  \brief The base media decoder for Torc.
 *
 * AudioDecoder is the default media demuxer and decoder for Torc.
 *
 * \sa VideoDecoder
 *
 * \todo Handle stream changes
 * \todo Flush all codecs after seek and/or flush codecs when changing selected stream
*/

AudioDecoder::AudioDecoder(const QString &URI, TorcPlayer *Parent, int Flags)
  : m_parent(Parent),
    m_audio(NULL),
    m_audioIn(new AudioDescription()),
    m_audioOut(new AudioDescription()),
    m_interruptDecoder(0),
    m_uri(URI),
    m_flags(Flags),
    m_priv(new AudioDecoderPriv(this)),
    m_seek(false),
    m_duration(0.0),
    m_bitrate(0),
    m_bitrateFactor(1),
    m_streamLock(new QReadWriteLock(QReadWriteLock::Recursive)),
    m_currentProgram(-1),
    m_chapterLock(new QReadWriteLock(QReadWriteLock::Recursive))
{
    // Global initialistaion
    InitialiseLibav();

    QReadLocker locker(m_streamLock);

    // Reset streams
    for (int i = 0; i < StreamTypeEnd; ++i)
        m_currentStreams[i] = -1;

    // audio
    if (m_parent)
    {
        AudioWrapper* wrapper = static_cast<AudioWrapper*>(m_parent->GetAudio());
        if (wrapper)
            m_audio = wrapper;
    }
}

AudioDecoder::~AudioDecoder()
{
    TearDown();
    delete m_chapterLock;
    delete m_streamLock;
    delete m_priv;
}

bool AudioDecoder::HandleAction(int Action)
{
    if (m_priv->m_buffer && m_priv->m_buffer->HandleAction(Action))
        return true;

    return false;
}

bool AudioDecoder::Open(void)
{
    if (m_uri.isEmpty())
        return false;

    m_priv->m_demuxerThread->start();
    QThread::usleep(50000);
    return true;
}

TorcDecoder::DecoderState AudioDecoder::GetState(void)
{
    return m_priv->m_demuxerThread->m_state;
}

TorcDecoder::DemuxerState AudioDecoder::GetDemuxerState(void)
{
    return m_priv->m_demuxerThread->m_demuxerState;
}

void AudioDecoder::SetDemuxerState(TorcDecoder::DemuxerState State)
{
    m_priv->m_demuxerThread->m_demuxerState = State;
}

void AudioDecoder::Start(void)
{
    m_priv->m_demuxerThread->Unpause();
}

void AudioDecoder::Pause(void)
{
    m_priv->m_demuxerThread->Pause();
}

void AudioDecoder::Stop(void)
{
    m_interruptDecoder = 1;
    m_priv->m_demuxerThread->Stop();
}

void AudioDecoder::Seek(void)
{
    if (!m_seek)
        m_seek = true;
}

/*! \brief Return the attachment for the given stream.
 *
 * The stream must be an attachment stream and the codec id must match the Type (there can be various types of
 * attachment streams in any given file). If there is a valid filename in the streams metadata, it will be used
 * to populate Name.
 *
 * An empty QByteArray is returned if the stream does not exist, the type does not match or there is no attachment.
*/
QByteArray AudioDecoder::GetAttachment(int Index, AVCodecID Type, QString &Name)
{
    QReadLocker locker(m_streamLock);

    if (m_programs.isEmpty())
        return QByteArray();

    // the underlying stream may have gone/moved...
    if (Index < 0 || Index >= m_programs[m_currentProgram]->m_streams[StreamTypeAttachment].size())
        return QByteArray();

    // don't return an unknown attachment type
    if (m_programs[m_currentProgram]->m_streams[StreamTypeAttachment][Index]->m_codecID != Type)
        return QByteArray();

    // fill in file name if known
    if (m_programs[m_currentProgram]->m_streams[StreamTypeAttachment][Index]->m_avMetaData.contains("filename"))
        Name = m_programs[m_currentProgram]->m_streams[StreamTypeAttachment][Index]->m_avMetaData.value("filename");

    return m_programs[m_currentProgram]->m_streams[StreamTypeAttachment][Index]->m_attachment;
}

/*! \brief Return the subtitle header for the subtitle stream referenced by Index.
 *
 * This is usually the ASS header.
 *
 * An empty QByteArray is returned if Index is invalid or there is no header.
*/
QByteArray AudioDecoder::GetSubtitleHeader(int Index)
{
    QReadLocker locker(m_streamLock);

    if (m_programs.isEmpty())
        return QByteArray();

    // the underlying stream may have gone/moved...
    if (Index < 0 || Index >= m_programs[m_currentProgram]->m_streams[StreamTypeSubtitle].size())
        return QByteArray();

    return m_programs[m_currentProgram]->m_streams[StreamTypeSubtitle][Index]->m_subtitleHeader;
}

int AudioDecoder::GetCurrentStream(TorcStreamTypes Type)
{
    QReadLocker locker(m_streamLock);
    return m_currentStreams[Type];
}

int AudioDecoder::GetStreamCount(TorcStreamTypes Type)
{
    int result = 0;
    m_streamLock->lockForRead();
    if (!m_programs.isEmpty())
        result = m_programs[m_currentProgram]->m_streams[Type].size();
    m_streamLock->unlock();
    return result;
}

/*! \brief Translate the given FFmpeg stream index to a Torc index for the current program and stream type.
*/
int AudioDecoder::GetTorcStreamIndex(int AvStreamIndex, TorcStreamTypes Type)
{
    int result = -1;

    QReadLocker locker(m_streamLock);

    if (m_programs.isEmpty())
        return result;

    for (int i = 0; i < m_programs[m_currentProgram]->m_streams[Type].size(); ++i)
    {
        if (m_programs[m_currentProgram]->m_streams[Type].at(i)->m_index == AvStreamIndex)
        {
            result = i;
            break;
        }
    }

    return result;
}

void AudioDecoder::DecodeVideoFrames(TorcVideoThread *Thread)
{
    if (!Thread)
        return;

    TorcDecoder::DecoderState*     state = &Thread->m_state;
    TorcDecoder::DecoderState* nextstate = &Thread->m_requestedState;
    TorcPacketQueue*               queue = Thread->m_queue;

    if (!queue)
        return;

    FlushVideoBuffers(false);
    *state = TorcDecoder::Running;
    bool yield = false;

    while (!m_interruptDecoder && *nextstate != TorcDecoder::Stopped)
    {
        queue->m_lock->lock();

        if (yield)
            queue->m_wait->wait(queue->m_lock);
        yield = true;

        if (m_interruptDecoder || *nextstate == TorcDecoder::Stopped)
        {
            queue->m_lock->unlock();
            break;
        }

        if (*nextstate == TorcDecoder::Running)
        {
            *nextstate = TorcDecoder::None;
            *state = TorcDecoder::Running;
        }

        if (*nextstate == TorcDecoder::Paused)
        {
            *nextstate = TorcDecoder::None;
            *state = TorcDecoder::Paused;
        }

        if (*state == TorcDecoder::Paused)
        {
            queue->m_lock->unlock();
            continue;
        }

        int unused, inuse, held = 0;
        bool uptodate = VideoBufferStatus(unused, inuse, held);

        if (uptodate)
            Thread->m_internalBufferEmpty = inuse < 1;

        if (unused < 1)
        {
            // TODO make this sleep dynamic
            queue->m_lock->unlock();
            QThread::usleep(4000);
            yield = !queue->Length() && uptodate;
            continue;
        }

        AVPacket *packet = NULL;

        if (queue->Length())
        {
            yield = false;
            packet = queue->Pop();
        }

        queue->m_lock->unlock();

        if (packet)
        {
            m_streamLock->lockForRead();

            int               index = m_currentStreams[StreamTypeVideo];
            AVStream        *stream = index > -1 ? m_priv->m_avFormatContext->streams[index] : NULL;
            AVCodecContext *context = stream ? stream->codec : NULL;

            if (packet == &gFlushCodec)
            {
                if (context && context->codec)
                    avcodec_flush_buffers(context);
                FlushVideoBuffers(false);
                packet = NULL;
            }
            else if (!stream || !context || index != packet->stream_index)
            {
                av_free_packet(packet);
                delete packet;
                packet = NULL;
            }

            if (packet)
            {
                ProcessVideoPacket(m_priv->m_avFormatContext, stream, packet);
                av_free_packet(packet);
                delete packet;
            }

            m_streamLock->unlock();
        }
    }

    {
        QReadLocker locker(m_streamLock);

        int index = m_currentStreams[StreamTypeVideo];
        AVStream *stream = index > -1 ? m_priv->m_avFormatContext->streams[index] : NULL;
        if (stream && stream->codec)
            avcodec_flush_buffers(stream->codec);
        CleanupVideoDecoder(stream);
    }

    FlushVideoBuffers(true);
    queue->Flush(true, false);

    {
        QWriteLocker locker(m_streamLock);
        m_currentStreams[StreamTypeVideo] = -1;
    }

    *state = TorcDecoder::Stopped;
}

bool AudioDecoder::VideoBufferStatus(int&, int&, int&)
{
    return true;
}

void AudioDecoder::ProcessVideoPacket(AVFormatContext*, AVStream*, AVPacket*)
{
}

AVCodec* AudioDecoder::PreInitVideoDecoder(AVFormatContext*, AVStream*)
{
    return NULL;
}

void AudioDecoder::PostInitVideoDecoder(AVCodecContext*)
{
}

void AudioDecoder::CleanupVideoDecoder(AVStream*)
{
}

void AudioDecoder::FlushVideoBuffers(bool)
{
}

void AudioDecoder::ProcessSubtitlePacket(AVFormatContext*, AVStream*, AVPacket*)
{
}

void AudioDecoder::DecodeAudioFrames(TorcAudioThread *Thread)
{
    if (!Thread)
        return;

    TorcDecoder::DecoderState*    state  = &Thread->m_state;
    TorcDecoder::DecoderState* nextstate = &Thread->m_requestedState;
    TorcPacketQueue* queue  = Thread->m_queue;

    if (!queue)
        return;

    qint64 lastpts = AV_NOPTS_VALUE;

    SetupAudio(Thread);
    uint8_t* audiosamples = (uint8_t *)av_mallocz(MAX_AUDIO_FRAME_SIZE * sizeof(int32_t));
    *state = TorcDecoder::Running;

    // loop at least once in case SetupAudio above takes a while and we've already
    // buffered the entire stream
    bool yield = false;

    while (!m_interruptDecoder && *nextstate != TorcDecoder::Stopped)
    {
        queue->m_lock->lock();

        if (yield)
            queue->m_wait->wait(queue->m_lock);
        yield = true;

        if (m_interruptDecoder || *nextstate == TorcDecoder::Stopped)
        {
            queue->m_lock->unlock();
            break;
        }

        if (*nextstate == TorcDecoder::Running)
        {
            *nextstate = TorcDecoder::None;
            *state = TorcDecoder::Running;
        }

        if (*nextstate == TorcDecoder::Paused)
        {
            *nextstate = TorcDecoder::None;
            *state = TorcDecoder::Paused;
        }

        if (*state == TorcDecoder::Paused)
        {
            queue->m_lock->unlock();
            continue;
        }

        // wait for the audio device
        int fill = m_audio ? m_audio->GetFillStatus() : 0;
        Thread->m_internalBufferEmpty = fill < 2;
        if (fill > (int)(m_audioOut->m_bestFillSize << 1))
        {
            queue->m_lock->unlock();
            QThread::usleep(m_audioOut->m_bufferTime * 500);
            yield = !queue->Length();
            continue;
        }

        AVPacket* packet = NULL;

        if (queue->Length())
        {
            yield  = false;
            packet = queue->Pop();
        }

        queue->m_lock->unlock();

        if (packet)
        {
            m_streamLock->lockForRead();

            int               index = m_currentStreams[StreamTypeAudio];
            AVStream        *stream = index > -1 ? m_priv->m_avFormatContext->streams[index] : NULL;
            AVCodecContext *context = stream ? stream->codec : NULL;

            if (packet == &gFlushCodec)
            {
                if (context && context->codec)
                    avcodec_flush_buffers(context);
                if (m_audio)
                    m_audio->Reset();
                packet = NULL;
                lastpts = AV_NOPTS_VALUE;
            }
            else if (!m_audio || !stream || !context || (m_audio && !m_audio->HasAudioOut()) ||
                     index != packet->stream_index)
            {
                av_free_packet(packet);
                delete packet;
                packet = NULL;
            }

            if (packet)
            {
                AVPacket temp;
                av_init_packet(&temp);
                temp.data = packet->data;
                temp.size = packet->size;

                bool reselectaudiotrack = false;

                while (temp.size > 0)
                {
                    int used = 0;
                    int datasize = 0;
                    int decodedsize = -1;
                    bool decoded = false;

                    if (!context->channels)
                    {
                        LOG(VB_GENERAL, LOG_INFO, QString("Setting channels to %1")
                            .arg(m_audioOut->m_channels));

                        bool shouldpassthrough = m_audio->ShouldPassthrough(context->sample_rate,
                                                                            context->channels,
                                                                            context->codec_id,
                                                                            context->profile,
                                                                            false);
                        if (shouldpassthrough || !m_audio->DecoderWillDownmix(context->codec_id))
                        {
                            // for passthrough of codecs for which the decoder won't downmix
                            // let the decoder set the number of channels. For other codecs
                            // we downmix if necessary in AudioOutput
                            context->request_channels = 0;
                        }
                        else // No passthru, the decoder will downmix
                        {
                            context->request_channels = m_audio->GetMaxChannels();
                            if (context->codec_id == AV_CODEC_ID_AC3)
                                context->channels = m_audio->GetMaxChannels();
                        }

                        used = m_priv->DecodeAudioPacket(context, audiosamples, datasize, &temp);
                        decodedsize = datasize;
                        decoded = true;
                        reselectaudiotrack |= (context->channels > 0);
                    }

                    if (reselectaudiotrack)
                    {
                        LOG(VB_GENERAL, LOG_WARNING, "Need to reselect audio track...");

                        m_streamLock->unlock();
                        if (SelectStream(StreamTypeAudio))
                            SetupAudio(Thread);
                        m_streamLock->lockForRead();

                        // just in case the context changed while we released the lock
                        index   = m_currentStreams[StreamTypeAudio];
                        stream  = index > -1 ? m_priv->m_avFormatContext->streams[index] : NULL;
                        context = stream ? stream->codec : NULL;
                    }

                    datasize = 0;

                    if (m_audioOut->m_passthrough)
                    {
                        if (!decoded)
                        {
                            if (m_audio->NeedDecodingBeforePassthrough())
                            {
                                used = m_priv->DecodeAudioPacket(context, audiosamples, datasize, &temp);
                                decodedsize = datasize;
                            }
                            else
                            {
                                decodedsize = -1;
                            }
                        }

                        memcpy(audiosamples, temp.data, temp.size);
                        datasize = temp.size;
                        temp.size = 0;
                    }
                    else
                    {
                        if (!decoded)
                        {
                            if (m_audio->DecoderWillDownmix(context->codec_id))
                            {
                                context->request_channels = m_audio->GetMaxChannels();
                                if (context->codec_id == AV_CODEC_ID_AC3)
                                    context->channels = m_audio->GetMaxChannels();
                            }

                            used = m_priv->DecodeAudioPacket(context, audiosamples, datasize, &temp);
                            decodedsize = datasize;
                        }

                        // When decoding some audio streams the number of
                        // channels, etc isn't known until we try decoding it.
                        if (context->sample_rate != m_audioOut->m_sampleRate ||
                            context->channels    != m_audioOut->m_channels)
                        {
                            // FIXME
                            LOG(VB_GENERAL, LOG_WARNING, QString("Audio stream changed (Samplerate %1->%2 channels %3->%4)")
                                .arg(m_audioOut->m_sampleRate).arg(context->sample_rate)
                                .arg(m_audioOut->m_channels).arg(context->channels));

                            m_streamLock->unlock();
                            if (SelectStream(StreamTypeAudio))
                                LOG(VB_GENERAL, LOG_INFO, "On same audio stream");
                            m_streamLock->lockForRead();

                            // just in case the context changed while we released the lock
                            index   = m_currentStreams[StreamTypeAudio];
                            stream  = index > -1 ? m_priv->m_avFormatContext->streams[index] : NULL;
                            context = stream ? stream->codec : NULL;

                            // FIXME - this is probably not wise
                            // try and let the buffer drain to avoid interruption
                            m_audio->Drain();

                            SetupAudio(Thread);
                            datasize = 0;
                        }
                    }

                    if (used < 0)
                    {
                        LOG(VB_GENERAL, LOG_ERR, "Unknown audio decoding error");
                        break;
                    }

                    if (datasize <= 0)
                    {
                        temp.data += used;
                        temp.size -= used;
                        continue;
                    }

                    int frames = (context->channels <= 0 || decodedsize < 0) ? -1 :
                        decodedsize / (context->channels * av_get_bytes_per_sample(context->sample_fmt));

                    qint64 pts = AV_NOPTS_VALUE;

                    if (packet->pts == (qint64)AV_NOPTS_VALUE)
                    {
                        if (lastpts == (qint64)AV_NOPTS_VALUE)
                            pts = 0;
                        else if (m_audioOut->m_passthrough && !m_audio->NeedDecodingBeforePassthrough())
                            pts = lastpts + m_audio->LengthLastData();
                        else
                            pts = lastpts + (long long)((double)(frames * 1000) / context->sample_rate);
                    }
                    else
                    {
                        pts = av_q2d(stream->time_base) * 1000 * packet->pts;
                    }

                    if (!FilterAudioFrames(pts))
                        m_audio->AddAudioData((char *)audiosamples, datasize, pts, frames);

                    temp.data += used;
                    temp.size -= used;

                    lastpts = pts;
                }
            }

            av_free_packet(packet);
            delete packet;

            m_streamLock->unlock();
        }
    }

    *state = TorcDecoder::Stopped;
    if (m_audio)
        m_audio->SetAudioOutput(NULL);
    av_free(audiosamples);
    queue->Flush(true, false);

    {
        QWriteLocker locker(m_streamLock);
        m_currentStreams[StreamTypeAudio] = -1;
    }
}

int AudioDecoderPriv::DecodeAudioPacket(AVCodecContext *Context, quint8 *Buffer,
                                        int &DataSize, AVPacket *Packet)
{
    AVFrame frame;
    avcodec_get_frame_defaults(&frame);

    int gotframe = 0;

    int result = avcodec_decode_audio4(Context, &frame, &gotframe, Packet);

    if (result < 0 || !gotframe)
    {
        if (result < 0)
            LOG(VB_GENERAL, LOG_ERR, QString("Error decoding audio: '%1'").arg(AVErrorToString(result)));

        DataSize = 0;
        return result;
    }

    int planar = av_sample_fmt_is_planar((AVSampleFormat)frame.format);
    DataSize   = av_samples_get_buffer_size(NULL, Context->channels, frame.nb_samples, (AVSampleFormat)frame.format, 1);

    if (planar)
    {
        if (!m_audioResampleContext)
            m_audioResampleContext = avresample_alloc_context();

        if (!m_audioResampleContext)
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to create audio resample context");
            DataSize = 0;
            return -1;
        }

        if (m_audioResampleChannelLayout != frame.channel_layout ||
            m_audioResampleFormat        != (AVSampleFormat)frame.format ||
            m_audioResampleSampleRate    != frame.sample_rate)
        {
            m_audioResampleChannelLayout = frame.channel_layout;
            m_audioResampleFormat        = (AVSampleFormat)frame.format;
            m_audioResampleSampleRate    = frame.sample_rate;
            avresample_close(m_audioResampleContext);
            av_opt_set_int(m_audioResampleContext, "in_channel_layout",  m_audioResampleChannelLayout, 0);
            av_opt_set_int(m_audioResampleContext, "in_sample_fmt",      m_audioResampleFormat, 0);
            av_opt_set_int(m_audioResampleContext, "in_sample_rate",     m_audioResampleSampleRate, 0);
            av_opt_set_int(m_audioResampleContext, "out_channel_layout", m_audioResampleChannelLayout, 0);
            av_opt_set_int(m_audioResampleContext, "out_sample_fmt",     av_get_packed_sample_fmt(m_audioResampleFormat), 0);
            av_opt_set_int(m_audioResampleContext, "out_sample_rate",    m_audioResampleSampleRate, 0);

            if (avresample_open(m_audioResampleContext) < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, "Failed to open audio resample context");
                DataSize = 0;
                return -1;
            }
        }

        int samples = avresample_convert(m_audioResampleContext, &Buffer,
                                         DataSize, frame.nb_samples,
                                         frame.extended_data,
                                         frame.linesize[0], frame.nb_samples);

        if (samples != frame.nb_samples)
            LOG(VB_GENERAL, LOG_WARNING, "Unexpected number of frames returned by avresample_convert");
    }
    else
    {
        memcpy(Buffer, frame.extended_data[0], DataSize);
    }

    return result;
}

bool AudioDecoder::FilterAudioFrames(qint64 Timecode)
{
    return false;
}

void AudioDecoder::DecodeSubtitles(TorcSubtitleThread *Thread)
{
    if (!Thread)
        return;

    TorcDecoder::DecoderState*    state  = &Thread->m_state;
    TorcDecoder::DecoderState* nextstate = &Thread->m_requestedState;
    TorcPacketQueue* queue  = Thread->m_queue;

    if (!queue)
        return;

    *state = TorcDecoder::Running;

    bool yield = false;

    while (!m_interruptDecoder && *nextstate != TorcDecoder::Stopped)
    {
        queue->m_lock->lock();

        if (yield)
            queue->m_wait->wait(queue->m_lock);
        yield = true;

        if (m_interruptDecoder || *nextstate == TorcDecoder::Stopped)
        {
            queue->m_lock->unlock();
            break;
        }

        if (*nextstate == TorcDecoder::Running)
        {
            *nextstate = TorcDecoder::None;
            *state = TorcDecoder::Running;
        }

        if (*nextstate == TorcDecoder::Paused)
        {
            *nextstate = TorcDecoder::None;
            *state = TorcDecoder::Paused;
        }

        AVPacket *packet = NULL;

        if (queue->Length())
        {
            yield = false;
            packet = queue->Pop();
        }

        queue->m_lock->unlock();

        if (packet)
        {
            m_streamLock->lockForRead();

            if (packet == &gFlushCodec)
            {
                uint numberstreams = m_priv->m_avFormatContext->nb_streams;
                for (uint i = 0; (numberstreams && (i < numberstreams)); ++i)
                    if (m_priv->m_avFormatContext->streams[i]->codec)
                        if (m_priv->m_avFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_SUBTITLE)
                            if (m_priv->m_avFormatContext->streams[i]->codec->codec)
                                avcodec_flush_buffers(m_priv->m_avFormatContext->streams[i]->codec);

                packet = NULL;
            }
            else
            {
                AVCodecID codecid = m_priv->m_avFormatContext->streams[packet->stream_index]->codec->codec_id;

                // teletext not supported (and may never be...)
                if (codecid != AV_CODEC_ID_DVB_TELETEXT)
                    ProcessSubtitlePacket(m_priv->m_avFormatContext, m_priv->m_avFormatContext->streams[packet->stream_index], packet);

                av_free_packet(packet);
                delete packet;
                packet = NULL;
            }

            m_streamLock->unlock();
        }
    }

    *state = TorcDecoder::Stopped;
    queue->Flush(true, false);

    {
        QWriteLocker locker(m_streamLock);
        m_currentStreams[StreamTypeSubtitle] = -1;
    }
}

void AudioDecoder::SetFlag(TorcDecoder::DecoderFlags Flag)
{
    m_flags |= Flag;
}

bool AudioDecoder::FlagIsSet(TorcDecoder::DecoderFlags Flag)
{
    return m_flags & Flag;
}

bool AudioDecoder::SelectProgram(int Index)
{
    QWriteLocker locker(m_streamLock);

    if (!(m_priv->m_demuxerThread->m_state == TorcDecoder::Opening ||
          m_priv->m_demuxerThread->m_state == TorcDecoder::Paused))
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot select program unless demuxer is paused");
        return false;
    }

    if (!m_priv->m_avFormatContext || Index >= m_programs.size() || Index < 0 || m_programs.isEmpty())
        return false;

    if (!m_priv->m_avFormatContext->nb_programs)
    {
        m_currentProgram = 0;
        return true;
    }

    uint avindex = m_programs[Index]->m_index;
    if (avindex >= m_priv->m_avFormatContext->nb_programs)
        avindex = 0;
    m_currentProgram = Index;

    for (uint i = 0; i < m_priv->m_avFormatContext->nb_programs; ++i)
        m_priv->m_avFormatContext->programs[i]->discard = (i == avindex) ? AVDISCARD_NONE : AVDISCARD_ALL;

    return true;
}

bool AudioDecoder::SelectStreams(void)
{
    if (m_priv->m_demuxerThread->m_state == TorcDecoder::Opening ||
        m_priv->m_demuxerThread->m_state == TorcDecoder::Paused)
    {
        SelectStream(StreamTypeAudio);
        SelectStream(StreamTypeVideo);

        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, "Cannot select streams unless demuxer is paused");
    return false;
}

void AudioDecoder::SetupAudio(TorcAudioThread *Thread)
{
    QReadLocker locker(m_streamLock);

    if (!m_priv->m_avFormatContext || !m_audio || !Thread)
        return;

    int index = m_currentStreams[StreamTypeAudio];

    if (index < 0 || index >= (int)m_priv->m_avFormatContext->nb_streams)
        return;

    TorcStreamData *stream  = NULL;
    AVCodecContext *context = m_priv->m_avFormatContext->streams[index]->codec;

    QList<TorcStreamData*>::iterator it = m_programs[m_currentProgram]->m_streams[StreamTypeAudio].begin();
    for ( ; it != m_programs[m_currentProgram]->m_streams[StreamTypeAudio].end(); ++it)
    {
        if ((*it)->m_index == index)
        {
            stream = (*it);
            break;
        }
    }

    if (!stream || !context)
    {
        LOG(VB_GENERAL, LOG_ERR, "Fatal audio error");
        return;
    }

    AudioFormat format = FORMAT_NONE;

    switch (av_get_packed_sample_fmt(context->sample_fmt))
    {
        case AV_SAMPLE_FMT_U8:
            format = FORMAT_U8;
            break;
        case AV_SAMPLE_FMT_S16:
            format = FORMAT_S16;
            break;
        case AV_SAMPLE_FMT_FLT:
            format = FORMAT_FLT;
            break;
        case AV_SAMPLE_FMT_DBL:
            format = FORMAT_NONE;
            break;
        case AV_SAMPLE_FMT_S32:
            switch (context->bits_per_raw_sample)
            {
                case  0: format = FORMAT_S32; break;
                case 24: format = FORMAT_S24; break;
                case 32: format = FORMAT_S32; break;
                default: format = FORMAT_NONE;
            }
            break;
        default:
            break;
    }

    if (format == FORMAT_NONE)
    {
        int bps = av_get_bytes_per_sample(context->sample_fmt) << 3;
        if (context->sample_fmt == AV_SAMPLE_FMT_S32 && context->bits_per_raw_sample)
            bps = context->bits_per_raw_sample;
        LOG(VB_GENERAL, LOG_ERR, QString("Unsupported sample format with %1 bits").arg(bps));
        return;
    }

    bool usingpassthrough = m_audio->ShouldPassthrough(context->sample_rate,
                                                       context->channels,
                                                       context->codec_id,
                                                       context->profile, false);

    context->request_channels = context->channels;
    if (!usingpassthrough && context->channels > (int)m_audio->GetMaxChannels() &&
        m_audio->DecoderWillDownmix(context->codec_id))
    {
        context->request_channels = m_audio->GetMaxChannels();
    }

    int samplesize   = context->channels * AudioOutputSettings::SampleSize(format);
    int codecprofile = context->codec_id == AV_CODEC_ID_DTS ? context->profile : 0;
    int originalchannels = stream->m_originalChannels;

    if (context->codec_id    == m_audioIn->m_codecId &&
        context->channels    == m_audioIn->m_channels &&
        samplesize           == m_audioIn->m_sampleSize &&
        context->sample_rate == m_audioIn->m_sampleRate &&
        format               == m_audioIn->m_format &&
        usingpassthrough     == m_audioIn->m_passthrough &&
        codecprofile         == m_audioIn->m_codecProfile &&
        originalchannels     == m_audioIn->m_originalChannels)
    {
        return;
    }

    *m_audioOut = AudioDescription(context->codec_id, format,
                                   context->sample_rate,
                                   context->channels,
                                   usingpassthrough,
                                   originalchannels,
                                   codecprofile);

    LOG(VB_GENERAL, LOG_INFO, "Audio format changed:");
    LOG(VB_GENERAL, LOG_INFO, "Old: " + m_audioIn->ToString());
    LOG(VB_GENERAL, LOG_INFO, "New: " + m_audioOut->ToString());

    *m_audioIn = *m_audioOut;
    m_audio->SetAudioParams(m_audioOut->m_format,
                            originalchannels,
                            context->request_channels,
                            m_audioOut->m_codecId,
                            m_audioOut->m_sampleRate,
                            m_audioOut->m_passthrough,
                            m_audioOut->m_codecProfile);
    m_audio->Initialise();
}

bool AudioDecoder::OpenDemuxer(TorcDemuxerThread *Thread)
{
    if (!Thread)
        return false;

    TorcDecoder::DecoderState *state = &Thread->m_state;

    if (*state > TorcDecoder::None)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Trying to reopen demuxer - ignoring");
        return false;
    }

    if (*state == TorcDecoder::Errored)
    {
        LOG(VB_GENERAL, LOG_INFO, "Trying to recreate demuxer");
        CloseDemuxer(Thread);
    }

    *state = TorcDecoder::Opening;

    // reset interrupt
    m_interruptDecoder = 0;

    // Create Torc buffer
    m_priv->m_buffer = TorcBuffer::Create(this, m_uri, &m_interruptDecoder, true);
    if (!m_priv->m_buffer)
    {
        CloseDemuxer(Thread);
        *state = TorcDecoder::Errored;
        return false;
    }

    // create AVFormatContext
    if (!CreateAVFormatContext(Thread))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to create AVFormatContext");
        CloseDemuxer(Thread);
        *state = TorcDecoder::Errored;
        return false;
    }

    if (!ScanStreams(Thread))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to scan streams");
        CloseDemuxer(Thread);
        *state = TorcDecoder::Errored;
    }

    // Ready
    *state = TorcDecoder::Paused;
    return true;
}

bool AudioDecoder::OpenDecoders(void)
{
    QReadLocker locker(m_streamLock);

    // Start afresh
    CloseDecoders();

    if (!m_priv->m_avFormatContext || m_programs.isEmpty())
        return false;

    if (m_flags & TorcDecoder::DecodeNone)
        return true;

    for (int i = StreamTypeStart; i < StreamTypeEnd; ++i)
    {
        if ((StreamTypeAudio == i) && !(m_flags & TorcDecoder::DecodeAudio))
        {
            continue;
        }

        if ((StreamTypeVideo == i || StreamTypeSubtitle == i) && !(m_flags & TorcDecoder::DecodeVideo))
        {
            continue;
        }

        if (!OpenDecoders(m_programs[m_currentProgram]->m_streams[i]))
        {
            CloseDecoders();
            return false;
        }
    }

    return true;
}

bool AudioDecoder::OpenDecoders(const QList<TorcStreamData*> &Streams)
{
    if (!m_priv->m_avFormatContext)
        return false;

    QList<TorcStreamData*>::const_iterator it = Streams.begin();
    for ( ; it != Streams.end(); ++it)
    {
        int index = (*it)->m_index;
        AVStream        *stream = m_priv->m_avFormatContext->streams[index];
        AVCodecContext *context = stream->codec;

        stream->discard = AVDISCARD_NONE;

        if (context->codec_id == AV_CODEC_ID_PROBE)
            continue;

        if (context->codec_type != AVMEDIA_TYPE_AUDIO &&
            context->codec_type != AVMEDIA_TYPE_VIDEO &&
            context->codec_type != AVMEDIA_TYPE_SUBTITLE)
        {
            continue;
        }

        if (context->codec_type == AVMEDIA_TYPE_SUBTITLE &&
           (context->codec_id   == AV_CODEC_ID_DVB_TELETEXT ||
            context->codec_id   == AV_CODEC_ID_TEXT))
        {
            continue;
        }

        AVCodec* avcodec = avcodec_find_decoder(context->codec_id);
        if (!avcodec)
        {
            QByteArray string(128, 0);
            avcodec_string(string.data(), 128, context, 0);
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to find decoder for stream #%1 %2")
                .arg(index).arg(string.data()));
            return false;
        }

        // NB 'post' initialisation is triggered when the first video packet is processed
        // by the decoder and AgreePixelFormat is called
        if (context->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            AVCodec* override = PreInitVideoDecoder(m_priv->m_avFormatContext, stream);
            if (override)
                avcodec = override;
        }

        int error;
        if ((error = avcodec_open2(context, avcodec, NULL)) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to open codec - error '%1'")
                .arg(AVErrorToString(error)));
            return false;
        }

        LOG(VB_GENERAL, LOG_INFO, QString("Stream #%1: Codec '%2' opened").arg(index).arg(avcodec->name));
    }

    return true;
}

void AudioDecoder::TearDown(void)
{
    Stop();
    m_priv->m_demuxerThread->Wait(1000);
}

/*! \brief Pause all decoding threads and wait for them to pause.
 *
 * Certain demuxer level operations require all processing to be paused.
*/
bool AudioDecoder::PauseDecoderThreads(TorcDemuxerThread* Thread)
{
    if (!Thread)
        return false;

    // pause
    Thread->m_videoThread->Pause();
    Thread->m_audioThread->Pause();
    Thread->m_subtitleThread->Pause();

    // and wait for pause
    int count = 0;
    while (!(Thread->m_videoThread->IsPaused() && Thread->m_audioThread->IsPaused() && Thread->m_subtitleThread->IsPaused()))
    {
        QThread::usleep(50000);
        if (count > 20)
        {
            LOG(VB_GENERAL, LOG_WARNING, "Waited too long for decoder threads to pause. Continuing");
            continue;
        }
    }

    return Thread->m_videoThread->IsPaused() && Thread->m_audioThread->IsPaused() && Thread->m_subtitleThread->IsPaused();
}

/*! \brief Reset the demuxer.
 *
 * If the buffer has signalled that there is a break in the stream (e.g. new title, sequence etc), then we need
 * to recreate our AVFormatContext and probe the stream again. Otherwise ffmpeg/libav will just add new streams on top of the old ones,
 * which are likely to now be empty/broken/redundant.
*/
bool AudioDecoder::ResetDemuxer(TorcDemuxerThread *Thread)
{
    if (!Thread)
        return false;

    LOG(VB_GENERAL, LOG_INFO, "Resetting demuxer for new sequence/clip");

    if (!PauseDecoderThreads(Thread))
        return false;

    // flush queues
    Thread->m_videoThread->m_queue->Flush(true, false);
    Thread->m_audioThread->m_queue->Flush(true, false);
    Thread->m_subtitleThread->m_queue->Flush(true, false);

    // mark this (demuxer) as paused
    TorcDecoder::DecoderState oldstate = Thread->m_state;
    Thread->m_state = TorcDecoder::Paused;

    ResetStreams(Thread);

    DeleteAVFormatContext(Thread);

    if (!CreateAVFormatContext(Thread))
        return false;

    if (!ScanStreams(Thread))
        return false;

    // ensure the demuxer is primed again
    Thread->m_state = oldstate;

    // restart decoding threads
    Thread->m_videoThread->Unpause();
    Thread->m_audioThread->Unpause();
    Thread->m_subtitleThread->Unpause();

    // testing
    LOG(VB_GENERAL, LOG_INFO, "Resetting audio");
    SetupAudio(Thread->m_audioThread);
    return true;
}

/*! \brief Update the demuxer for new streams.
 *
 * FFmpeg/libav will never remove streams but new streams may be added (usually for PIDs that were not
 * found in the initial probe).
 *
 * \todo This may be too heavy handed. If we've done everything else correctly, this should only need to action the new streams.
*/
bool AudioDecoder::UpdateDemuxer(TorcDemuxerThread *Thread)
{
    if (!Thread)
        return false;

    LOG(VB_GENERAL, LOG_INFO, "Updating demuxer for new streams");

    if (!PauseDecoderThreads(Thread))
        return false;

    // flush queues (no point in flushing the codec as the threads are paused)
    Thread->m_videoThread->m_queue->Flush(true, false);
    Thread->m_audioThread->m_queue->Flush(true, false);
    Thread->m_subtitleThread->m_queue->Flush(true, false);

    // flush codec internal buffers
    for (uint i = 0; i < m_priv->m_avFormatContext->nb_streams; i++)
    {
        AVCodecContext *codec = m_priv->m_avFormatContext->streams[i]->codec;
        if (codec && codec->codec)
            avcodec_flush_buffers(codec);
    }

    // mark this (demuxer) as paused
    TorcDecoder::DecoderState oldstate = Thread->m_state;
    Thread->m_state = TorcDecoder::Paused;

    ResetStreams(Thread);

    // (re)perform any post-initialisation
    m_priv->m_buffer->InitialiseAVContext(m_priv->m_avFormatContext);

    if (!ScanStreams(Thread))
        return false;

    // ensure the demuxer is primed again
    Thread->m_state = oldstate;

    // restart decoding threads
    Thread->m_videoThread->Unpause();
    Thread->m_audioThread->Unpause();
    Thread->m_subtitleThread->Unpause();

    // testing
    LOG(VB_GENERAL, LOG_INFO, "Resetting audio");
    SetupAudio(Thread->m_audioThread);

    return true;
}

void AudioDecoder::CloseDemuxer(TorcDemuxerThread *Thread)
{
    // Stop the consumer threads
    if (Thread)
    {
        Thread->m_videoThread->Stop();
        Thread->m_audioThread->Stop();
        Thread->m_subtitleThread->Stop();
        Thread->m_videoThread->Wait(1000);
        Thread->m_audioThread->Wait(1000);
        Thread->m_subtitleThread->Wait(1000);
    }

    ResetStreams(Thread);
    DeleteAVFormatContext(Thread);

    // Delete Torc buffer
    delete m_priv->m_buffer;
    m_priv->m_buffer = NULL;
}

bool AudioDecoder::CreateAVFormatContext(TorcDemuxerThread *Thread)
{
    if (!Thread || !m_priv || !m_priv->m_buffer)
        return false;

    DeleteAVFormatContext(Thread);

    AVInputFormat *format = NULL;
    bool needbuffer = true;

    if (m_priv->m_buffer->RequiredAVContext())
    {
        AVFormatContext *required = (AVFormatContext*)m_priv->m_buffer->RequiredAVContext();
        if (required)
        {
            m_priv->m_avFormatContext = required;
            m_priv->m_createdAVFormatContext = false;
            LOG(VB_GENERAL, LOG_INFO, "Buffer has already allocated decoder context");
        }
    }

    if (!m_priv->m_avFormatContext)
    {
        AVInputFormat *required = (AVInputFormat*)m_priv->m_buffer->RequiredAVFormat(needbuffer);
        if (required)
        {
            format = required;
            LOG(VB_GENERAL, LOG_INFO, QString("Demuxer required by buffer '%1'").arg(format->name));
        }

        m_priv->m_createdAVFormatContext = true;

        if (!format)
        {
            // probe (only necessary for some files)
            int probesize = PROBE_BUFFER_SIZE;
            if (!m_priv->m_buffer->IsSequential() && m_priv->m_buffer->BytesAvailable() < probesize)
                probesize = m_priv->m_buffer->BytesAvailable();
            probesize += AVPROBE_PADDING_SIZE;

            QByteArray *probebuffer = new QByteArray(probesize, 0);
            AVProbeData probe;
            probe.filename = m_uri.toLocal8Bit().constData();
            probe.buf_size = m_priv->m_buffer->Peek((quint8*)probebuffer->data(), probesize);
            probe.buf      = (unsigned char*)probebuffer->data();
            format = av_probe_input_format(&probe, 0);
            delete probebuffer;

            if (format)
                format->flags &= ~AVFMT_NOFILE;
        }

        // Allocate AVFormatContext
        m_priv->m_avFormatContext = avformat_alloc_context();
        if (!m_priv->m_avFormatContext)
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to allocate format context.");
            return false;
        }
    }

    // abort callback
    m_priv->m_avFormatContext->interrupt_callback.opaque = (void*)&m_interruptDecoder;
    m_priv->m_avFormatContext->interrupt_callback.callback = AudioDecoder::DecoderInterrupt;

    if (needbuffer)
    {
        // Create libav buffer
        if (m_priv->m_libavBuffer)
            av_free(m_priv->m_libavBuffer);

        m_priv->m_libavBufferSize = m_priv->m_buffer->BestBufferSize();
        if (!m_priv->m_buffer->IsSequential() && m_priv->m_buffer->BytesAvailable() < m_priv->m_libavBufferSize)
            m_priv->m_libavBufferSize = m_priv->m_buffer->BytesAvailable();
        m_priv->m_libavBuffer = (unsigned char*)av_mallocz(m_priv->m_libavBufferSize + FF_INPUT_BUFFER_PADDING_SIZE);

        if (!m_priv->m_libavBuffer)
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to create ffmpeg buffer");
            return false;
        }

        LOG(VB_GENERAL, LOG_INFO, QString("Input buffer size: %1 bytes").arg(m_priv->m_libavBufferSize));

        // Create libav byte context
        m_priv->m_avFormatContext->pb = avio_alloc_context(m_priv->m_libavBuffer,
                                                           m_priv->m_libavBufferSize,
                                                           0, m_priv->m_buffer,
                                                           &TorcBuffer::StaticRead,
                                                           &TorcBuffer::StaticWrite,
                                                           &TorcBuffer::StaticSeek);

        m_priv->m_avFormatContext->pb->seekable = !m_priv->m_buffer->IsSequential();
    }

    // Open
    if (m_priv->m_createdAVFormatContext)
    {
        int err = 0;
        QString uri = m_priv->m_buffer->GetFilteredUri();
        if ((err = avformat_open_input(&m_priv->m_avFormatContext, uri.toLocal8Bit().constData(), format, NULL)) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to open AVFormatContext - error '%1' (%2)").arg(AVErrorToString(err)).arg(uri));
            return false;
        }

        // Scan for streams
        if ((err = avformat_find_stream_info(m_priv->m_avFormatContext, NULL)) < 0)
            LOG(VB_GENERAL, LOG_WARNING, QString("Failed to find streams - error '%1'").arg(AVErrorToString(err)));

        // perform any post-initialisation
        m_priv->m_buffer->InitialiseAVContext(m_priv->m_avFormatContext);
    }

    return true;
}

///brief Delete AVFormatContext if we created it
void AudioDecoder::DeleteAVFormatContext(TorcDemuxerThread *Thread)
{
    (void)Thread;

    // Delete AVFormatContext (and byte context)
    if (m_priv->m_avFormatContext && m_priv->m_createdAVFormatContext)
    {
        LOG(VB_GENERAL, LOG_INFO, "Deleting AVFormatContext");
        avformat_close_input(&m_priv->m_avFormatContext);
    }
    m_priv->m_avFormatContext = NULL;
    m_priv->m_createdAVFormatContext = false;

    // NB this should have been deleted in avformat_close_input
    m_priv->m_libavBuffer = NULL;
    m_priv->m_libavBufferSize = 0;
}

void AudioDecoder::ResetStreams(TorcDemuxerThread *Thread)
{
    (void)Thread;

    // Reset stream selection
    {
        QWriteLocker locker(m_streamLock);
        for (int i = 0; i < StreamTypeEnd; ++i)
            m_currentStreams[i] = -1;
    }

    // Close stream decoders
    CloseDecoders();

    // Release program details
    ResetPrograms();

    // Clear chapters
    ResetChapters();

    // reset some internals
    m_seek = false;
    m_duration = 0.0;
    m_bitrate = 0;
    m_bitrateFactor = 1;
}

bool AudioDecoder::ScanStreams(TorcDemuxerThread *Thread)
{
    (void)Thread;

    // Scan programs
    if (!ScanPrograms())
    {
        // This is currently most likely to happen with MHEG only streams, BDJ menus etc
        LOG(VB_GENERAL, LOG_WARNING, "Failed to find any valid programs");
    }
    else
    {
        // Get the bitrate
        UpdateBitrate();

        // Select a program
        if (SelectProgram(0))
        {
            // Select streams
            if (SelectStreams())
            {
                // Open decoders
                if (!OpenDecoders())
                {
                    LOG(VB_GENERAL, LOG_ERR, "Failed to open decoders");
                    return false;
                }

                // Parse chapters
                ScanChapters();

                // Debug!
                DebugPrograms();
            }
        }
    }

    return true;
}

void AudioDecoder::DemuxPackets(TorcDemuxerThread *Thread)
{
    if (!Thread)
        return;

    TorcDecoder::DecoderState* state     = &Thread->m_state;
    TorcDecoder::DecoderState* nextstate = &Thread->m_requestedState;

    bool eof          = false;
    bool waseof       = false;
    bool demuxererror = false;
    AVPacket *packet  = NULL;

    while (!m_interruptDecoder && m_priv->m_avFormatContext && *nextstate != TorcDecoder::Stopped)
    {
        if (*state == TorcDecoder::Pausing)
        {
            if (Thread->m_audioThread->IsPaused() &&
                Thread->m_videoThread->IsPaused() &&
                Thread->m_subtitleThread->IsPaused())
            {
                LOG(VB_PLAYBACK, LOG_INFO, "Demuxer paused");
                *state = TorcDecoder::Paused;
                continue;
            }

            QThread::usleep(10000);
            continue;
        }

        if (*state == TorcDecoder::Starting)
        {
            // Start the consumer threads
            if (!Thread->m_audioThread->IsRunning())
                Thread->m_audioThread->start();

            if (!Thread->m_videoThread->IsRunning())
                Thread->m_videoThread->start();

            if (!Thread->m_subtitleThread->IsRunning())
                Thread->m_subtitleThread->start();

            if (Thread->m_audioThread->IsPaused()    || !Thread->m_audioThread->IsRunning() ||
                Thread->m_videoThread->IsPaused()    || !Thread->m_videoThread->IsRunning() ||
                Thread->m_subtitleThread->IsPaused() || !Thread->m_subtitleThread->IsRunning())
            {
                QThread::usleep(10000);
                continue;
            }

            LOG(VB_PLAYBACK, LOG_INFO, "Demuxer started");
            *state = TorcDecoder::Running;
            continue;
        }

        if (*nextstate == TorcDecoder::Paused)
        {
            LOG(VB_PLAYBACK, LOG_INFO, "Demuxer pausing...");
            Thread->m_videoThread->Pause();
            Thread->m_audioThread->Pause();
            Thread->m_subtitleThread->Pause();

            if (*state == TorcDecoder::Running)
                m_priv->m_pauseResult = av_read_pause(m_priv->m_avFormatContext);
            *state = TorcDecoder::Pausing;
            *nextstate = TorcDecoder::None;
            continue;
        }

        if (*nextstate == TorcDecoder::Running)
        {
            LOG(VB_PLAYBACK, LOG_INFO, "Demuxer unpausing...");
            Thread->m_videoThread->Unpause();
            Thread->m_audioThread->Unpause();
            Thread->m_subtitleThread->Unpause();

            av_read_play(m_priv->m_avFormatContext);
            *state = TorcDecoder::Starting;
            *nextstate = TorcDecoder::None;
            continue;
        }

        if (m_seek)
        {
            qint64 timestamp = 0;
            int result = av_seek_frame(m_priv->m_avFormatContext, -1, timestamp, 0);
            if (result < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Failed to seek - error '%1'")
                    .arg(AVErrorToString(result)));
            }
            else
            {
                // NB this only flushes the currently selected streams
                Thread->m_videoThread->m_queue->Flush(true, true);
                Thread->m_audioThread->m_queue->Flush(true, true);
                Thread->m_subtitleThread->m_queue->Flush(true, true);
            }

            m_seek = false;
        }

        if (*state == TorcDecoder::Paused)
        {
            QThread::usleep(10000);
            continue;
        }

        if (Thread->m_audioThread->m_queue->Size() > MAX_QUEUE_SIZE_AUDIO ||
            Thread->m_videoThread->m_queue->Size() > MAX_QUEUE_SIZE_VIDEO)
        {
            Thread->m_videoThread->m_queue->m_wait->wakeAll();
            Thread->m_audioThread->m_queue->m_wait->wakeAll();
            Thread->m_subtitleThread->m_queue->m_wait->wakeAll();
            QThread::usleep(50000);
            continue;
        }

        if (!packet)
        {
            packet = new AVPacket;
            memset(packet, 0, sizeof(AVPacket));
            av_init_packet(packet);
        }

        // N.B. no need to lock m_streamLock for read from demux thread
        int videoindex = m_currentStreams[StreamTypeVideo];
        int audioindex = m_currentStreams[StreamTypeAudio];

        if (eof)
        {
            if (!waseof)
            {
                waseof = true;

                if (videoindex > -1)
                {
                    av_init_packet(packet);
                    packet->data = NULL;
                    packet->size = 0;
                    packet->stream_index = videoindex;
                    Thread->m_videoThread->m_queue->Push(packet);
                    Thread->m_videoThread->m_queue->Flush(false, true);
                }

                if (audioindex > -1)
                {
                    if (m_priv->m_avFormatContext->streams[audioindex]->codec->codec->capabilities & CODEC_CAP_DELAY)
                    {
                        av_init_packet(packet);
                        packet->data = NULL;
                        packet->size = 0;
                        packet->stream_index = audioindex;
                        Thread->m_audioThread->m_queue->Push(packet);
                    }
                }
            }

            if ((Thread->m_audioThread->m_queue->Length() +
                 Thread->m_videoThread->m_queue->Length() +
                 Thread->m_subtitleThread->m_queue->Length()) == 0)
            {
                if (!Thread->m_audioThread->m_internalBufferEmpty ||
                    !Thread->m_videoThread->m_internalBufferEmpty ||
                    !Thread->m_subtitleThread->m_internalBufferEmpty)
                {
                    Thread->m_videoThread->m_queue->m_wait->wakeAll();
                    Thread->m_audioThread->m_queue->m_wait->wakeAll();
                    Thread->m_subtitleThread->m_queue->m_wait->wakeAll();
                    QThread::usleep(50000);
                    continue;
                }

                break;
            }
            else
            {
                QThread::usleep(50000);
                continue;
            }
        }

        uint oldstreamcount = m_priv->m_avFormatContext->nb_streams;

        int error;
        if ((error = av_read_frame(m_priv->m_avFormatContext, packet)) < 0)
        {
            // the buffer has reached the end of a sequence/clip and needs to synchronise with the player
            if (m_priv->m_demuxerThread->m_demuxerState == TorcDecoder::DemuxerWaiting && (error == TORC_AVERROR_FLUSH || error == TORC_AVERROR_RESET))
            {
                // unset eof
                m_priv->m_avFormatContext->pb->eof_reached = 0;

                LOG(VB_GENERAL, LOG_INFO, "Waiting for player buffers to drain");

                int count = 0;
                while (!m_interruptDecoder && m_priv->m_demuxerThread->m_demuxerState == TorcDecoder::DemuxerWaiting)
                {
                    if (count++ > 400)
                    {
                        LOG(VB_GENERAL, LOG_WARNING, "Waited 20 seconds for player to drain buffers - continuing");
                        m_priv->m_demuxerThread->m_demuxerState = TorcDecoder::DemuxerReady;
                        break;
                    }

                    QThread::usleep(50000);
                }

                // exit immediately if needed
                if (m_interruptDecoder)
                    break;
            }
            else if (error == TORC_AVERROR_IDLE)
            {
                m_priv->m_avFormatContext->pb->eof_reached = 0;
                QThread::usleep(50000);
                continue;
            }
            else
            {
                if (error == AVERROR_EOF || m_priv->m_avFormatContext->pb->eof_reached)
                {
                    LOG(VB_GENERAL, LOG_INFO, "End of file");
                    eof = true;
                    continue;
                }

                if (m_priv->m_avFormatContext->pb->error)
                {
                    int ioerror = m_priv->m_avFormatContext->pb->error;

                    if (!(ioerror == TORC_AVERROR_FLUSH || ioerror == TORC_AVERROR_IDLE || ioerror == TORC_AVERROR_RESET))
                    {
                        LOG(VB_GENERAL, LOG_ERR, QString("libav io error (%1)").arg(AVErrorToString(ioerror)));
                        demuxererror = true;
                        break;
                    }
                }

                QThread::usleep(50000);
                continue;
            }
        }

        // if the demuxer has been waiting for player buffers to drain, it may need to be flushed BEFORE
        // we process any more packets
        if (error == TORC_AVERROR_FLUSH)
        {
            Thread->m_videoThread->m_queue->Flush(true, true);
            Thread->m_audioThread->m_queue->Flush(true, true);
            Thread->m_subtitleThread->m_queue->Flush(true, true);
        }

        if (Q_UNLIKELY(error == TORC_AVERROR_RESET || ((oldstreamcount != m_priv->m_avFormatContext->nb_streams) && (m_priv->m_avFormatContext->ctx_flags & AVFMTCTX_NOHEADER))))
        {
            uint newstreamcount = m_priv->m_avFormatContext->nb_streams;
            LOG(VB_GENERAL, LOG_INFO, QString("Stream count changed %1->%2").arg(oldstreamcount).arg(newstreamcount));

            if (newstreamcount == 0)
            {
                LOG(VB_GENERAL, LOG_ERR, "No streams found. Exiting");
                *nextstate = TorcDecoder::Stopped;
                continue;
            }

            if (error == TORC_AVERROR_RESET)
            {
                if (!ResetDemuxer(Thread))
                {
                    LOG(VB_GENERAL, LOG_ERR, "Failed to reset demuxer. Exiting");
                    *nextstate = TorcDecoder::Stopped;
                    continue;
                }
            }
            else if (!UpdateDemuxer(Thread))
            {
                LOG(VB_GENERAL, LOG_ERR, "Failed to update demuxer. Exiting");
                *nextstate = TorcDecoder::Stopped;
                continue;
            }
        }

        if (packet->stream_index == videoindex)
            Thread->m_videoThread->m_queue->Push(packet);
        else if (packet->stream_index == audioindex)
            Thread->m_audioThread->m_queue->Push(packet);
        else if (m_priv->m_avFormatContext->streams[packet->stream_index]->codec->codec_type == AVMEDIA_TYPE_SUBTITLE)
            Thread->m_subtitleThread->m_queue->Push(packet);
        else
            av_free_packet(packet);
    }

    *state = TorcDecoder::Stopping;
    LOG(VB_GENERAL, LOG_INFO, "Demuxer stopping");
    Thread->m_videoThread->Stop();
    Thread->m_audioThread->Stop();
    Thread->m_subtitleThread->Stop();
    Thread->m_videoThread->Wait();
    Thread->m_audioThread->Wait();
    Thread->m_subtitleThread->Wait();

    *state = TorcDecoder::Stopped;
    LOG(VB_GENERAL, LOG_INFO, "Demuxer stopped");

    while (!m_interruptDecoder && !demuxererror && *nextstate != TorcDecoder::Stopped)
        QThread::usleep(50000);

    m_interruptDecoder = 1;
    LOG(VB_GENERAL, LOG_INFO, "Demuxer exiting");

    CloseDemuxer(Thread);

    if (demuxererror)
        *state = TorcDecoder::Errored;
}

void AudioDecoder::CloseDecoders(void)
{
    QReadLocker locker (m_streamLock);

    if (!m_priv->m_avFormatContext)
        return;

    for (uint i = 0; i < m_priv->m_avFormatContext->nb_streams; i++)
    {
        m_priv->m_avFormatContext->streams[i]->discard = AVDISCARD_ALL;
        if (m_priv->m_avFormatContext->streams[i]->codec)
        {
            if (m_priv->m_avFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
                CleanupVideoDecoder(m_priv->m_avFormatContext->streams[i]);
            avcodec_close(m_priv->m_avFormatContext->streams[i]->codec);
        }
    }
}

bool AudioDecoder::ScanPrograms(void)
{
    // Reset
    ResetPrograms();

    // Sanity check
    if (!m_priv->m_avFormatContext)
        return false;

    QWriteLocker locker(m_streamLock);

    // Top level metadata
    if (m_priv->m_avFormatContext->metadata && av_dict_count(m_priv->m_avFormatContext->metadata))
    {
        AVDictionaryEntry *entry = NULL;
        while ((entry = av_dict_get(m_priv->m_avFormatContext->metadata, "", entry, AV_DICT_IGNORE_SUFFIX)))
            m_avMetaData.insert(QString::fromUtf8(entry->key).trimmed(), QString::fromUtf8(entry->value).trimmed());
    }

    // Programs (usually none or one)
    if (m_priv->m_avFormatContext->nb_programs)
    {
        for (uint i = 0; i < m_priv->m_avFormatContext->nb_programs; ++i)
        {
            TorcProgramData* program = ScanProgram(i);
            if (program)
                m_programs.append(program);
        }
    }
    else
    {
        TorcProgramData* program = new TorcProgramData();
        for (uint i = 0; i < m_priv->m_avFormatContext->nb_streams; ++i)
        {
            TorcStreamData* stream = ScanStream(i);
            if (stream)
            {
                program->m_streamCount++;
                program->m_streams[stream->m_type].append(stream);
            }
        }

        if (program->IsValid())
            m_programs.append(program);
        else
            delete program;
    }

    return m_programs.size() > 0;
}

TorcProgramData* AudioDecoder::ScanProgram(uint Index)
{
    if (!m_priv->m_avFormatContext)
        return NULL;

    if (Index >= m_priv->m_avFormatContext->nb_programs)
        return NULL;

    TorcProgramData* program = new TorcProgramData();
    AVProgram* avprogram = m_priv->m_avFormatContext->programs[Index];

    // Id
    program->m_index = Index;
    program->m_id = avprogram->id;

    // Metadata
    if (avprogram->metadata && av_dict_count(avprogram->metadata))
    {
        AVDictionaryEntry *entry = NULL;
        while ((entry = av_dict_get(avprogram->metadata, "", entry, AV_DICT_IGNORE_SUFFIX)))
            program->m_avMetaData.insert(QString::fromUtf8(entry->key).trimmed(), QString::fromUtf8(entry->value).trimmed());
    }

    // Streams
    for (uint i = 0; i < avprogram->nb_stream_indexes; ++i)
    {
        TorcStreamData* stream = ScanStream(avprogram->stream_index[i]);
        if (stream)
        {
            program->m_streamCount++;
            program->m_streams[stream->m_type].append(stream);
        }
    }

    if (!program->IsValid())
    {
        delete program;
        return NULL;
    }

    return program;
}

void AudioDecoder::ResetPrograms(void)
{
    QWriteLocker locker(m_streamLock);

    // Clear top level metadata
    m_avMetaData.clear();

    // Clear programs
    while (!m_programs.isEmpty())
        delete m_programs.takeLast();

    // Default to the first program
    m_currentProgram = 0;
}

TorcStreamData* AudioDecoder::ScanStream(uint Index)
{
    if (!m_priv->m_avFormatContext)
        return NULL;

    if (Index >= m_priv->m_avFormatContext->nb_streams)
        return NULL;

    TorcStreamData* stream  = new TorcStreamData();
    AVStream* avstream      = m_priv->m_avFormatContext->streams[Index];
    stream->m_index         = Index;
    stream->m_id            = avstream->id;
    stream->m_avDisposition = avstream->disposition;
    stream->m_codecID       = avstream->codec->codec_id;

    // Metadata
    if (avstream->metadata && av_dict_count(avstream->metadata))
    {
        AVDictionaryEntry *entry = NULL;
        while ((entry = av_dict_get(avstream->metadata, "", entry, AV_DICT_IGNORE_SUFFIX)))
            stream->m_avMetaData.insert(QString::fromUtf8(entry->key).trimmed(), QString::fromUtf8(entry->value).trimmed());
    }

    // Language
    if (stream->m_avMetaData.contains("language"))
        stream->m_language = TorcLanguage::From3CharCode(stream->m_avMetaData["language"]);

    // Types...
    if (avstream->disposition & AV_DISPOSITION_ATTACHED_PIC)
    {
        stream->m_type = StreamTypeAttachment;
    }
    else
    {
        switch (avstream->codec->codec_type)
        {
            case AVMEDIA_TYPE_VIDEO:
                stream->m_type = StreamTypeVideo;
                break;
            case AVMEDIA_TYPE_AUDIO:
                stream->m_type = StreamTypeAudio;
                stream->m_originalChannels = avstream->codec->channels;
                break;
            case AVMEDIA_TYPE_SUBTITLE:
                stream->m_type = StreamTypeSubtitle;
                stream->m_subtitleHeader = QByteArray((char *)avstream->codec->subtitle_header, avstream->codec->subtitle_header_size);
                break;
            case AVMEDIA_TYPE_ATTACHMENT:
                stream->m_type = StreamTypeAttachment;
                stream->m_attachment = QByteArray((char *)avstream->codec->extradata, avstream->codec->extradata_size);
                break;
            case AVMEDIA_TYPE_DATA:
                stream->m_type = StreamTypeUnknown;
                break;
            default:
                stream->m_type = StreamTypeUnknown;
                break;
        }
    }

    // video resolution
    if (avstream->codec->codec_type == AVMEDIA_TYPE_VIDEO)
    {
        stream->m_width  = avstream->codec->width;
        stream->m_height = avstream->codec->height;
    }

    if (!stream->IsValid())
    {
        delete stream;
        return NULL;
    }

    return stream;
}

void AudioDecoder::ScanChapters(void)
{
    ResetChapters();

    QWriteLocker locker(m_chapterLock);

    if (m_priv->m_avFormatContext && m_priv->m_avFormatContext->nb_chapters > 1)
    {
        for (uint i = 0; i < m_priv->m_avFormatContext->nb_chapters; ++i)
        {
            AVChapter* avchapter = m_priv->m_avFormatContext->chapters[i];
            TorcChapter* chapter = new TorcChapter();
            chapter->m_id = avchapter->id;
            chapter->m_startTime = (qint64)((long double)avchapter->start *
                                            (long double)avchapter->time_base.num /
                                            (long double)avchapter->time_base.den);

            if (avchapter->metadata && av_dict_count(avchapter->metadata))
            {
                AVDictionaryEntry *entry = NULL;
                while ((entry = av_dict_get(avchapter->metadata, "", entry, AV_DICT_IGNORE_SUFFIX)))
                    chapter->m_avMetaData.insert(QString::fromUtf8(entry->key).trimmed(), QString::fromUtf8(entry->value).trimmed());
            }

            m_chapters.append(chapter);
        }
    }
}

void AudioDecoder::ResetChapters(void)
{
    QWriteLocker locker(m_chapterLock);

    while (!m_chapters.isEmpty())
        delete m_chapters.takeLast();
}

bool AudioDecoder::SelectStream(TorcStreamTypes Type)
{
    QWriteLocker locker(m_streamLock);

    if (m_programs.isEmpty())
        return false;

    int current  = m_currentStreams[Type];
    int selected = -1;
    int count    = m_programs[m_currentProgram]->m_streams[Type].size();
    bool ignore = (Type == StreamTypeAudio && !FlagIsSet(DecodeAudio)) ||
                  ((Type == StreamTypeVideo || Type == StreamTypeSubtitle) && !FlagIsSet(DecodeVideo));

    // no streams available
    if (count < 1 || ignore)
    {
        m_currentStreams[Type] = selected;
        return current == selected;
    }

    // only one available
    if (count == 1)
    {
        selected = m_programs[m_currentProgram]->m_streams[Type][0]->m_index;
        m_currentStreams[Type] = selected;
        return current == selected;
    }

    // pick one
    QLocale::Language language = gLocalContext->GetLanguage();
    int index = 0;
    int score = 0;


    QList<TorcStreamData*>::iterator it = m_programs[m_currentProgram]->m_streams[Type].begin();
    for ( ; it != m_programs[m_currentProgram]->m_streams[Type].end(); ++it)
    {
        bool languagematch = (language != DEFAULT_QT_LANGUAGE) && ((*it)->m_language == language);
        bool forced        = (*it)->m_avDisposition & AV_DISPOSITION_FORCED;
        bool defaultstream = (*it)->m_avDisposition & AV_DISPOSITION_DEFAULT;
        int thisscore = (count - index) + (languagematch ? 500 : 0) +
                        (forced ? 1000 : 0) + (defaultstream ? 100 : 0) +
                        (((*it)->m_originalChannels + count) * 2) +
                        (((*it)->m_width * (*it)->m_height) >> 16);

        if (thisscore > score)
        {
            score = thisscore;
            selected = (*it)->m_index;
        }
        index++;
    }

    m_currentStreams[Type] = selected;
    return current == selected;
}

void AudioDecoder::UpdateBitrate(void)
{
    m_duration = 0.0;
    m_bitrate = 0;
    m_bitrateFactor = 1;

    if (!m_priv->m_avFormatContext)
        return;

    m_duration = (double)m_priv->m_avFormatContext->duration / ((double)AV_TIME_BASE);

    m_bitrate = m_priv->m_avFormatContext->bit_rate;
    if (QString(m_priv->m_avFormatContext->iformat->name).contains("matroska", Qt::CaseInsensitive))
        m_bitrateFactor = 2;

    if (m_bitrate < 1000 && m_duration > 0)
    {
        qint64 filesize = m_priv->m_buffer->GetSize();
        m_bitrate = (filesize << 4) / m_duration;
        LOG(VB_GENERAL, LOG_INFO, "Guessing bitrate from file size and duration");
    }

    if (m_bitrate < 1000)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Unable to determine a reasonable bitrate - forcing");
        m_bitrate = 1000000;
    }

    if (m_priv->m_buffer)
        m_priv->m_buffer->SetBitrate(m_bitrate, m_bitrateFactor);
}

void AudioDecoder::DebugPrograms(void)
{
    if (!m_priv->m_avFormatContext)
        return;

    QReadLocker locker(m_streamLock);

    // General
    LOG(VB_GENERAL, LOG_INFO, QString("Demuxer '%1' for '%2'").arg(m_priv->m_avFormatContext->iformat->name).arg(m_uri));
    LOG(VB_GENERAL, LOG_INFO, QString("Duration: %1 Bitrate: %2 kbit/s")
        .arg(AVTimeToString(m_priv->m_avFormatContext->duration))
        .arg(m_priv->m_avFormatContext->bit_rate / 1000));

    // Chapters
    if (m_chapters.size() > 1)
    {
        for (int i = 0; i < m_chapters.size(); ++i)
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Chapter #%1 [%2] start: %3")
                .arg(i).arg(m_chapters[i]->m_id).arg(m_chapters[i]->m_startTime));
            if (m_chapters[i]->m_avMetaData.size())
            {
                LOG(VB_GENERAL, LOG_INFO, "Metadata:");
                QMap<QString,QString>::iterator it = m_chapters[i]->m_avMetaData.begin();
                for ( ; it != m_chapters[i]->m_avMetaData.end(); ++it)
                    LOG(VB_GENERAL, LOG_INFO, QString("\t%1:%2").arg(it.key(), -12, ' ').arg(it.value(), -12, ' '));
            }
        }
    }

    // Metadata
    if (m_avMetaData.size())
    {
        LOG(VB_GENERAL, LOG_INFO, "Metadata:");
        QMap<QString,QString>::iterator it = m_avMetaData.begin();
        for ( ; it != m_avMetaData.end(); ++it)
            LOG(VB_GENERAL, LOG_INFO, QString("\t%1:%2").arg(it.key(), -12, ' ').arg(it.value(), -12, ' '));
    }

    // Programs
    for (int i = 0; i < m_programs.size(); ++i)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Program #%1").arg(m_programs[i]->m_id));

        if (m_programs[i]->m_avMetaData.size())
        {
            LOG(VB_GENERAL, LOG_INFO, "Metadata:");
            QMap<QString,QString>::iterator it = m_programs[i]->m_avMetaData.begin();
            for ( ; it != m_programs[i]->m_avMetaData.end(); ++it)
                LOG(VB_GENERAL, LOG_INFO, QString("\t%1:%2").arg(it.key(), -12, ' ').arg(it.value(), -12, ' '));
        }

        // Streams
        DebugStreams(m_programs[i]->m_streams[StreamTypeVideo]);
        DebugStreams(m_programs[i]->m_streams[StreamTypeAudio]);
        DebugStreams(m_programs[i]->m_streams[StreamTypeSubtitle]);
        DebugStreams(m_programs[i]->m_streams[StreamTypeAttachment]);
    }
}

void AudioDecoder::DebugStreams(const QList<TorcStreamData*> &Streams)
{
    QList<TorcStreamData*>::const_iterator it = Streams.begin();
    for ( ; it != Streams.end(); ++it)
    {
        QByteArray string(128, 0);
        avcodec_string(string.data(), 128, m_priv->m_avFormatContext->streams[(*it)->m_index]->codec, 0);

        LOG(VB_GENERAL, LOG_INFO, QString("Stream #%1 %2[0x%3] %4 %5")
            .arg((*it)->m_index)
            .arg(StreamTypeToString((*it)->m_type))
            .arg((*it)->m_id, 0, 16)
            .arg(TorcLanguage::ToString((*it)->m_language, true))
            .arg(string.data()));
    }
}

class TorcAudioDecoderFactory : public TorcDecoderFactory
{
    void Score(int DecodeFlags, const QString &URI, int &Score, TorcPlayer *Parent)
    {
        if (DecodeFlags & TorcDecoder::DecodeVideo)
            return;

        if (Score <= 50)
            Score = 50;
    }

    TorcDecoder* Create(int DecodeFlags, const QString &URI, int &Score, TorcPlayer *Parent)
    {
        if (DecodeFlags & TorcDecoder::DecodeVideo)
            return NULL;

        if (Score > 50)
            return NULL;

        return new AudioDecoder(URI, Parent, DecodeFlags);
    }
} TorcAudioDecoderFactory;
