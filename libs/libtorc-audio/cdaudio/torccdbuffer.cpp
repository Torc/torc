// Torc
#include "torclogging.h"
#include "torccdbuffer.h"

TorcCDBuffer::TorcCDBuffer(const QString &URI, int *Abort)
  : TorcBuffer(URI, Abort),
    m_input(NULL)
{
}

TorcCDBuffer::~TorcCDBuffer()
{
}

void* TorcCDBuffer::RequiredAVFormat(void)
{
    return m_input;
}

void TorcCDBuffer::InitialiseAVContext(void* Context)
{
    // with FFmpeg 1.2, cd playback fails as the dts is not set. This affects ffplay as well.
    AVFormatContext* context = static_cast<AVFormatContext*>(Context);
    if (context && context->nb_streams > 0)
        context->streams[0]->cur_dts = 0;
}

QString TorcCDBuffer::GetFilteredUri(void)
{
    return m_uri.mid(3);
}

bool TorcCDBuffer::Open(void)
{
    m_input = av_find_input_format("libcdio");

    if (!m_input)
        LOG(VB_GENERAL, LOG_ERR, "libcdio demuxer not found");

    return m_input;
}

void TorcCDBuffer::Close(void)
{
}

int TorcCDBuffer::Read(quint8 *Buffer, qint32 BufferSize)
{
    (void)Buffer;
    (void)BufferSize;
    return 0;
}

int TorcCDBuffer::Peek(quint8 *Buffer, qint32 BufferSize)
{
    (void)Buffer;
    (void)BufferSize;
    return 0;
}

int TorcCDBuffer::Write(quint8 *Buffer, qint32 BufferSize)
{
    (void)Buffer;
    (void)BufferSize;
    return -1;
}

int64_t TorcCDBuffer::Seek(int64_t Offset, int Whence)
{
    (void)Offset;
    (void)Whence;
    return 0;
}

qint64 TorcCDBuffer::GetSize(void)
{
    return -1;
}

qint64 TorcCDBuffer::GetPosition(void)
{
    return 0;
}

bool TorcCDBuffer::IsSequential(void)
{
    return false;
}

qint64 TorcCDBuffer::BytesAvailable(void)
{
    return 0;
}

int TorcCDBuffer::BestBufferSize(void)
{
    return 0;
}

static class TorcCDBufferFactory : public TorcBufferFactory
{
    void Score(const QString &URI, const QUrl &URL, int &Score, const bool &Media)
    {
        if (Media && URI.startsWith("cd:", Qt::CaseInsensitive) && Score <= 20)
            Score = 20;
    }

    TorcBuffer* Create(const QString &URI, const QUrl &URL, const int &Score, int *Abort, const bool &Media)
    {
        if (Media && URI.startsWith("cd:", Qt::CaseInsensitive) && Score <= 20)
            return new TorcCDBuffer(URI, Abort);

        return NULL;
    }

} TorcCDBufferFactory;
