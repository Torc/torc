// Torc
#include "torclogging.h"
#include "torccdbuffer.h"

TorcCDBuffer::TorcCDBuffer(const QString &URI)
  : TorcBuffer(URI),
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
    TorcBuffer* Create(const QString &URI, const QUrl &URL)
    {
        if (URI.startsWith("cd:", Qt::CaseInsensitive))
        {
            TorcCDBuffer* result = new TorcCDBuffer(URI);
            if (result->Open())
                return result;

            delete result;
        }

        return NULL;
    }

} TorcCDBufferFactory;
