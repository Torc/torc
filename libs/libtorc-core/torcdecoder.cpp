/* Class TorcDecoder
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
#include "torclogging.h"
#include "torcdecoder.h"

TorcDecoder::~TorcDecoder()
{
}

TorcDecoder* TorcDecoder::Create(int DecodeFlags, const QString &URI, TorcPlayer *Parent)
{
    TorcDecoder *decoder = NULL;

    DecoderFactory* factory = DecoderFactory::GetDecoderFactory();
    for ( ; factory; factory = factory->NextFactory())
    {
        decoder = factory->Create(DecodeFlags, URI, Parent);
        if (decoder)
            break;
    }

    if (!decoder)
        LOG(VB_GENERAL, LOG_ERR, "Failed to create decoder");

    return decoder;
}

DecoderFactory* DecoderFactory::gDecoderFactory = NULL;

DecoderFactory::DecoderFactory()
{
    nextDecoderFactory = gDecoderFactory;
    gDecoderFactory = this;
}

DecoderFactory::~DecoderFactory()
{
}

DecoderFactory* DecoderFactory::GetDecoderFactory(void)
{
    return gDecoderFactory;
}

DecoderFactory* DecoderFactory::NextFactory(void) const
{
    return nextDecoderFactory;
}
