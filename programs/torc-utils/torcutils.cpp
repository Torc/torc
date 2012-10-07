// Torc
#include "torcexitcodes.h"
#include "torcdecoder.h"
#include "torcutils.h"

int TorcUtils::Probe(const UtilsCommandLineParser *Cmdline)
{
    if (!Cmdline)
        return GENERIC_EXIT_NOT_OK;

    QString uri = Cmdline->ToString("infile");

    if (uri.isEmpty())
        return GENERIC_EXIT_INVALID_CMDLINE;

    int flags = TorcDecoder::DecodeNone;
    TorcDecoder* decoder = new TorcDecoder(uri, flags);
    if (decoder->Open())
    {
        while (!(decoder->State() == TorcDecoder::Stopped ||
                 decoder->State() == TorcDecoder::Paused ||
                 decoder->State() == TorcDecoder::Errored))
        {
            usleep(50000);
        }
    }

    delete decoder;
    return GENERIC_EXIT_OK;
}
