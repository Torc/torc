/* Class TorcDecoder
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
#include "torcdecoder.h"

/*! \class TorcDecoder
 *  \brief The base decoder interface.
 *
 * With the exception of the static Create function, TorcDecoder is a pure interface class.
 *
 * Concrete subclasses must implement Open, Start, Stop, Pause etc.
 *
 * \sa AudioDecoder
 * \sa VideoDecoder
 * \sa TorcPlayer
 * \sa TorcDecoderFactory
*/

TorcDecoder::~TorcDecoder()
{
    LOG(VB_GENERAL, LOG_INFO, "Decoder destroyed");
}

/*! \enum  TorcDecoder::DecoderFlags
 *  \brief Enumeration to enable various decoder capabilities.
 *  \var TorcDecoder::DecoderFlags TorcDecoder::DecodeNone
 *  Decode nothing.
 *  \var TorcDecoder::DecoderFlags TorcDecoder::DecodeAudio
 *  Decode audio tracks.
 *  \var TorcDecoder::DecoderFlags TorcDecoder::DecodeVideo
 *  Decode video tracks.
 *  \var TorcDecoder::DecoderFlags TorcDecoder::DecodeForcedTracksOn
 *  Enable decoding of forced subtitle tracks.
 *  \var TorcDecoder::DecoderFlags TorcDecoder::DecodeSubtitlesAlwaysOn
 *  Always decode subtitle tracks.
 *  \var TorcDecoder::DecoderFlags TorcDecoder::DecodeMultithreaded
 *  Use multiple threads for decoding.
 *
 * \enum  TorcDecoder::DecoderState
 *  \brief The current internal state of the decoder.
 *  \var TorcDecoder::DecoderState TorcDecoder::Errored
 *  The decoder has encountered an unrecoverable error.
 *  \var TorcDecoder::DecoderState TorcDecoder::None
 *  The default, undefined state.
 *  \var TorcDecoder::DecoderState TorcDecoder::Opening
 *  The decoder is being created.
 *  \var TorcDecoder::DecoderState TorcDecoder::Paused
 *  The decoder is paused.
 *  \var TorcDecoder::DecoderState TorcDecoder::Starting
 *  The decoder is transitioning from the Paused to Running state.
 *  \var TorcDecoder::DecoderState TorcDecoder::Running
 *  The decoder is running.
 *  \var TorcDecoder::DecoderState TorcDecoder::Pausing
 *  The decoder is transitioning from the Running to the Paused state.
 *  \var TorcDecoder::DecoderState TorcDecoder::Stopping
 *  The decoder is transitioning from the Running to the Stopped state.
 *  \var TorcDecoder::DecoderState TorcDecoder::Stopped
 *  The decode is stopped (prior to deletion).
 *
 *  \fn    TorcDecoder::HandleAction
 *  \brief Handle TorcEvent type actions
 *
 * Implementations should first pass actions to any relevant child classes (such as
 * TorcBuffer).
 *
 * \returns True if the action has been actioned, false otherwise.
 *
 * \fn    TorcDecoder::Open
 * \brief Open the decoder
 *
 * \returns True on succes, false otherwise.
 *
 * \fn TorcDecoder::GetState
 * \returns The current internal state of the decoder.
 *
 * \fn TorcDecoder::Start
 * \brief Start the decoder if it is not already running.
 *
 * \fn TorcDecoder::Stop
 * \brief Stop the decoder if it is not already stopped.
 *
 * \fn TorcDecoder::Pause
 * \brief Pause the decoder.
 *
 * \fn TorcDecoder::Seek
 * \brief Seek within the current program.
 *
 * \fn TorcDecoder::GetStreamCount
 * \return The number of media streams of a given type in the current program.
 *
 * \fn TorcDecoder::GetCurrentStream
 * \return The current stream number for the given type in the current program.
*/

/*! \brief Create a decoder object to handle the object described by URI.
 *
 * Create iterates over the list of registered TorcDecoderFactory subclasses and will
 * attempt to Create the most suitable TorcDecoder subclass that can handle the media
 * described by URI.
 *
 * \param DecodeFlags High level decoder functionality required by the player.
 * \param URI         A Uniform Resource Identifier pointing to the object to be opened.
 * \param Parent      The parent player for the decoder.
*/
TorcDecoder* TorcDecoder::Create(int DecodeFlags, const QString &URI, TorcPlayer *Parent)
{
    TorcDecoder *decoder = NULL;

    int score = 0;
    TorcDecoderFactory* factory = TorcDecoderFactory::GetTorcDecoderFactory();
    for ( ; factory; factory = factory->NextFactory())
        (void)factory->Score(DecodeFlags, URI, score, Parent);

    factory = TorcDecoderFactory::GetTorcDecoderFactory();
    for ( ; factory; factory = factory->NextFactory())
    {
        decoder = factory->Create(DecodeFlags, URI, score, Parent);
        if (decoder)
            break;
    }

    if (!decoder)
        LOG(VB_GENERAL, LOG_ERR, "Failed to create decoder");

    return decoder;
}

/*! \class TorcDecoderFactory
 *  \brief Base class for adding a TorcDecoder implementation.
 *
 * To make a TorcDecoder subclass available for use within an application, subclass TorcDecoderFactory
 * and implement Score and Create. Implementations can offer new decoder functionality and ensure
 * they are selected over the default decoder class by increasing their 'score'.
 *
 * \sa TorcDecoder
 * \sa TorcAudioDecoderFactory
 * \sa TorcVideoDecoderFactory
*/

TorcDecoderFactory* TorcDecoderFactory::gTorcDecoderFactory = NULL;

/// The base contstructor adds this object to the head of the linked list of TorcDecoderFactory concrete implementations.
TorcDecoderFactory::TorcDecoderFactory()
{
    nextTorcDecoderFactory = gTorcDecoderFactory;
    gTorcDecoderFactory = this;
}

TorcDecoderFactory::~TorcDecoderFactory()
{
}

/*! \fn    TorcDecoderFactory::Score
 *  \brief Assess whether the item described by by URI can be handled.
 *
 * DecodeFlags and URI describe an item of media that requires decoding in a specific manner.
 * If the TorcDecoder managed by this factory can handle this decoding, Score is set appropriately.
 *
 * \sa Create
*/

/*! \fn    TorcDecoderFactory::Create
 *  \brief Create a TorcDecoder object capable of decoding the media described by URI.
 *
 * If Score is less than the value set by the associated Score implementation, do not create an
 * object and return NULL.
 *
 * \sa Score
*/

/*! \fn    TorcDecoderFactory::GetTorcDecoderFactory
 *  \brief Returns a pointer to the first item in the linked list of TorcDecoderFactory objects.
*/
TorcDecoderFactory* TorcDecoderFactory::GetTorcDecoderFactory(void)
{
    return gTorcDecoderFactory;
}

TorcDecoderFactory* TorcDecoderFactory::NextFactory(void) const
{
    return nextTorcDecoderFactory;
}
