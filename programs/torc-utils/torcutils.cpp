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

int TorcUtils::Probe(const UtilsCommandLineParser *Cmdline)
{
    if (!Cmdline)
        return GENERIC_EXIT_NOT_OK;

    QString uri = Cmdline->ToString("infile");

    if (uri.isEmpty())
        return GENERIC_EXIT_INVALID_CMDLINE;

    TorcDecoder* decoder = TorcDecoder::Create(TorcDecoder::DecodeNone, uri, NULL);
    if (decoder && decoder->Open())
    {
        while (!(decoder->State() == TorcDecoder::Stopped ||
                 decoder->State() == TorcDecoder::Paused ||
                 decoder->State() == TorcDecoder::Errored))
        {
            TorcUSleep(50000);
        }
    }

    delete decoder;
    return GENERIC_EXIT_OK;
}

int TorcUtils::Play(const UtilsCommandLineParser *Cmdline)
{
    int result = GENERIC_EXIT_NOT_OK;
    if (!Cmdline)
        return result;

    QString uri = Cmdline->ToString("infile");

    if (uri.isEmpty())
        return GENERIC_EXIT_INVALID_CMDLINE;

    AudioInterface* interface = new AudioInterface(NULL, true);
    if (interface->InitialisePlayer())
    {
        interface->SetURI(uri);
        if (interface->PlayMedia(false))
            result = qApp->exec();
    }

    delete interface;
    return result;
}
