/* Class TorcBuffer
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012-13
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
#include "torcbuffer.h"

/*! \class TorcBuffer
 *  \brief The base class for opening media files and streams.
 *
 * Concrete subclasses must implement Read, Peek, Write, Seek, GetSize, Get Position,
 * IsSequential, BytesAvailable and BestBufferSize and are registered for use by subclassing
 * TorcBufferFactory.
 *
 * \sa TorcFileBuffer
 * \sa TorcNetworkBuffer
 * \sa TorcBufferFactory
*/

/*! \fn TorcBuffer::TorcBuffer
 *
 * Protected to enforce creation through the factory classes via TorcBuffer::Create
*/
TorcBuffer::TorcBuffer(const QString &URI, int *Abort)
  : m_uri(URI),
    m_path(URI),
    m_state(Status_Created),
    m_paused(true),
    m_bitrate(0),
    m_bitrateFactor(1),
    m_abort(Abort)
{
}

TorcBuffer::~TorcBuffer()
{
}

/*! \fn    TorcBuffer::Create
 *  \brief Create a buffer object that can handle the object described by URI.
 *
 * Create iterates over the list of registered TorcBufferFactory subclasses and will
 * attempt to Create and Open the most suitable TorcBuffer subclass that can handle the media
 * described by URI.
 *
 * \param URI   A Uniform Resource Identifier pointing to the object to be opened.
 * \param Abort A flag to indicate to the buffer object that it should cease as soon as possible.
 * \param Media Set to true to hint to the buffer that this is a media (audio/video) file and may require special treatment.
*/
TorcBuffer* TorcBuffer::Create(const QString &URI, int *Abort, bool Media)
{
    TorcBuffer* buffer = NULL;
    QUrl url(URI);

    int score = 0;
    TorcBufferFactory* factory = TorcBufferFactory::GetTorcBufferFactory();
    for ( ; factory; factory = factory->NextTorcBufferFactory())
        (void)factory->Score(URI, url, score, Media);

    factory = TorcBufferFactory::GetTorcBufferFactory();
    for ( ; factory; factory = factory->NextTorcBufferFactory())
    {
        buffer = factory->Create(URI, url, score, Abort, Media);
        if (buffer)
        {
            if (buffer->Open())
                break;
            delete buffer;
            buffer = NULL;
        }
    }

    if (!buffer)
        LOG(VB_GENERAL, LOG_WARNING, QString("Failed to create buffer for '%1'").arg(URI));

    return buffer;
}

/*! \fn    TorcBuffer::RequiredAVContext
 *  \brief This buffer object has its own internal AVFormatContext.
 *
 * TorcBuffer subclasses that require their own AVFormatContext for parsing media should reimplement
 * this method.
 *
 * \sa RequiredAVFormat
*/
void* TorcBuffer::RequiredAVContext(void)
{
    return NULL;
}

/*! \fn    TorcBuffer::RequiredAVFormat
 *  \brief This buffer object has its own AVFormat.
 *
 * TorcBuffer subclasses that have their own AVFormat should reimplement this method.
 *
 * \sa RequiredAVContext
*/
void* TorcBuffer::RequiredAVFormat(void)
{
    return NULL;
}

/*! \fn    TorcBuffer::StaticRead
 *  \brief Read from the buffer Object.
 *
 * Do not call this function directly. It acts as part of the interface between an
 * AVFormatContext structure and the underlying TorcBuffer that supplies it with data.
*/
int TorcBuffer::StaticRead(void *Object, quint8 *Buffer, qint32 BufferSize)
{
    TorcBuffer* buffer = static_cast<TorcBuffer*>(Object);

    if (buffer)
        return buffer->Read(Buffer, BufferSize);

    return -1;
}

/*! \fn    TorcBuffer::StaticWrite
 *  \brief Write tothe buffer Object.
 *
 * Do not call this function directly. It acts as part of the interface between an
 * AVFormatContext structure and the underlying TorcBuffer that supplies it with data.
*/
int TorcBuffer::StaticWrite(void *Object, quint8 *Buffer, qint32 BufferSize)
{
    TorcBuffer* buffer = static_cast<TorcBuffer*>(Object);

    if (buffer)
        return buffer->Write(Buffer, BufferSize);

    return -1;
}

/*! \fn    TorcBuffer::StaticSeek
 *  \brief Seek within the buffer Object.
 *
 * Do not call this function directly. It acts as part of the interface between an
 * AVFormatContext structure and the underlying TorcBuffer that supplies it with data.
*/
int64_t TorcBuffer::StaticSeek(void *Object, int64_t Offset, int Whence)
{
    TorcBuffer* buffer = static_cast<TorcBuffer*>(Object);

    if (buffer)
        return buffer->Seek(Offset, Whence);

    return -1;
}

/*! \fn    TorcBuffer::Open
 *  \brief Open the buffer.
 *
 * This method should be reimplemented in any subclass and called explicitly at the end of
 * any reimplementation.
*/
bool TorcBuffer::Open(void)
{
    LOG(VB_GENERAL, LOG_INFO, QString("Opened '%1'").arg(m_uri));
    return Unpause();
}

/*! \fn    TorcBuffer::Close
 *  \brief Close the buffer
 *
 * This method should be reimplemented in any subclass and called explicitly at the end of
 * any reimplementation.
*/
void TorcBuffer::Close(void)
{
    LOG(VB_GENERAL, LOG_INFO, QString("Closing '%1'").arg(m_uri));
}

/*! \fn    TorcBuffer::HandleAction
 *  \brief Process events relevant to this buffer.
 *
 * Buffer objects may be passed Torc actions. As an example, a DVD buffer may receive keypress related
 * actions passed from the UI to the player to the decoder and ultimately to the underlying buffer.
 *
 * N.B. This method is not thread safe and concrete subclasses should take appropriate measures.
 *
 * \returns True if this buffer has handled the action, otherwise false.
*/
bool TorcBuffer::HandleAction(int Action)
{
    return false;
}

/*! \fn    TorcBuffer::ReadAll
 *  \brief Read the entire contents of the buffer into main memory.
 *
 * This is a convenience method to synchronously read the entire buffer into main memory. It
 * should be used with caution as it may block.
 *
 * \param Timeout The maximum number of Milliseconds to wait before returning.
*/
QByteArray TorcBuffer::ReadAll(int Timeout /*=0*/)
{
    (void)Timeout;
    return QByteArray();
}

/*! \fn    TorcBuffer::Pause
 *  \brief Pause the buffer.
 *
 * Reimplement this method in a subclass if the buffer needs to take specific action to
 * physically pause a device or external resource.
 *
 * \returns True if the objects state was changed to Status_Paused, false otherwise.
*/
bool TorcBuffer::Pause(void)
{
    if (m_paused)
        return false;

    m_paused = true;
    m_state  = Status_Paused;

    return true;
}


/*! \fn    TorcBuffer::Unpause
 *  \brief Unpause the buffer.
 *
 * Reimplement this method in a subclass if the buffer needs to take specific action to
 * physically unpause a device or external resource.
 *
 * \returns True if the objects state was changed to Status_Opened, false otherwise.
*/
bool TorcBuffer::Unpause(void)
{
    if (!m_paused)
        return false;

    m_paused = false;
    m_state  = Status_Opened;
    return true;
}

/*! \fn    TorcBuffer::TogglePause
 *  \brief Toggle the pause state of the buffer.
 *
 * \returns True if the objects state was changed to Status_Paused, false otherwise.
*/
bool TorcBuffer::TogglePause(void)
{
    if (m_paused)
        return Unpause();
    return Pause();
}

/*! \fn     TorcBuffer::GetPaused
 * \returns True if the objects is Paused, false otherwise.
*/
bool TorcBuffer::GetPaused(void)
{
    return m_paused;
}

/*! \fn    TorcBuffer::SetBitrate
 *  \brief Set the estimated bitrate for this media (audio/video) file.
 *
 * \attention m_bitrate is not yet used.
*/
void TorcBuffer::SetBitrate(int Bitrate, int Factor)
{
    m_bitrate = Bitrate;
    m_bitrateFactor = Factor;

    LOG(VB_GENERAL, LOG_INFO, QString("New bitrate: %1 kbit/s (factor %2)")
        .arg(m_bitrate / 1000).arg(m_bitrateFactor));
}

/*! \fn    TorcBuffer::GetFilteredUri
 *  \brief Get the filtered Uri for this buffer.
 *
 * Reimplement this to handle the removal of any media identifiers and return a string
 * that can be passed directly to other functions for use.
 *
 * \returns QString with any leading identifiers removed (e.g. cd: or dvd:)
*/
QString TorcBuffer::GetFilteredUri(void)
{
    return m_uri;
}

/*! \fn    TorcBuffer::GetPath
 *  \brief Get the path portion of this buffers URI.
 *
 * Reimplement this to return the path to this object or set m_path when the buffer
 * is created or opened.
*/
QString TorcBuffer::GetPath(void)
{
    return m_path;
}

/*! \fn    TorcBuffer::GetURI
 *  \brief Get the URI for this buffer.
*/
QString TorcBuffer::GetURI(void)
{
    return m_uri;
}

TorcBufferFactory* TorcBufferFactory::gTorcBufferFactory = NULL;

/*! \class TorcBufferFactory
 *  \brief Base class for adding a TorcBuffer implementation.
 *
 * To make a TorcBuffer subclass available for use within an application, subclass TorcBufferFactory
 * and implement Score and Create. Implementations can offer functionality to handle new buffer types
 * or extend/improve functionality for existing TorcBuffer types and increase its 'score' appropriately
 * to ensure it is the preferred choice in TorcBuffer::Create.
 *
 * \sa TorcBuffer
*/

/*! \fn   TorcBufferFactory::TorcBufferFactory
 *
 * The base contstructor adds this object to the head of the linked list of TorcBufferFactory concrete
 * implementations.
*/
TorcBufferFactory::TorcBufferFactory()
{
    m_nextTorcBufferFactory = gTorcBufferFactory;
    gTorcBufferFactory = this;
}

TorcBufferFactory::~TorcBufferFactory()
{
}

/*! \fn    TorcBufferFactory::Score
 *  \brief Assess whether the item described by by URI can be handled.
 *
 * URI and URL provide details on a media object that is to be opened. If the buffer type for which
 * this TorcBufferFactory implementation is responsible is capable of opening this media, set Score
 * to an appropriate value. The higher the score, the better the handling.
 *
 * Default handlers will use a score of 50.
 *
 * \sa Create
*/

/*! \fn    TorcBufferFactory::Create
 *  \brief Create a TorcBuffer object capable of opening the media described by URI.
 *
 * If Score is less than the value set by the associated Score implementation, do not create an
 * object and return NULL.
 *
 * \sa Score
*/

/*! \fn    TorcBufferFactory::GetTorcBufferFactory
 *  \brief Returns a pointer to the first item in the linked list of TorcBufferFactory objects.
*/
TorcBufferFactory* TorcBufferFactory::GetTorcBufferFactory(void)
{
    return gTorcBufferFactory;
}

/*! \fn    TorcBufferFactory::NextTorcBufferFactory
 *  \brief Return a pointer to the next TorcBufferFactory static class.
*/
TorcBufferFactory* TorcBufferFactory::NextTorcBufferFactory(void) const
{
    return m_nextTorcBufferFactory;
}
