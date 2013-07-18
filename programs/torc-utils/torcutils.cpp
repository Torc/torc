// Qt
#include <QCoreApplication>

// Torc
#include "torcexitcodes.h"
#include "torccoreutils.h"
#include "torcdecoder.h"
#include "torcplayer.h"
#include "audiointerface.h"
#include "audiooutput.h"
#include "torcutils.h"

int TorcUtils::Probe(const QString &URI)
{
    TorcDecoder* decoder = TorcDecoder::Create(TorcDecoder::DecodeNone, URI, NULL);
    if (decoder && decoder->Open())
    {
        while (!(decoder->GetState() == TorcDecoder::Stopped ||
                 decoder->GetState() == TorcDecoder::Paused ||
                 decoder->GetState() == TorcDecoder::Errored))
        {
            TorcCoreUtils::USleep(50000);
        }
    }

    delete decoder;
    return GENERIC_EXIT_OK;
}

int TorcUtils::Play(const QString &URI)
{
    int result = GENERIC_EXIT_OK;

    AudioInterface* interface = new AudioInterface(NULL, true);
    if (interface->InitialisePlayer())
    {
        interface->SetURI(URI);
        if (interface->PlayMedia(false))
            result = qApp->exec();
    }

    delete interface;
    return result;
}
