// Qt
#include <QCoreApplication>
#include <QThread>

// Torc
#include "version.h"
#include "torclocalcontext.h"
#include "torcexitcodes.h"
#include "torcutils.h"
#include "torccommandline.h"


int main(int argc, char **argv)
{
    new QCoreApplication(argc, argv);
    // the 'torc-' prefix is used for identification purposes elsewhere, don't change it
    QCoreApplication::setApplicationName("torc-utils");
    QThread::currentThread()->setObjectName(TORC_MAIN_THREAD);

    int ret = GENERIC_EXIT_OK;

    {
        QScopedPointer<TorcCommandLine> cmdline(new TorcCommandLine(TorcCommandLine::URI));
        if (!cmdline.data())
            return GENERIC_EXIT_NOT_OK;

        cmdline->Add("probe", QVariant(), "Probe the given URI for media content (audio, video and still images).", TorcCommandLine::None);
        cmdline->Add("play",  QVariant(), "Play the given URI.", TorcCommandLine::None);

        bool justexit = false;
        ret = cmdline->Evaluate(argc, argv, justexit);

        if (ret != GENERIC_EXIT_OK)
            return ret;

        if (justexit)
            return ret;

        if (int error = TorcLocalContext::Create(cmdline.data(), Torc::Network))
            return error;

        QString uri = cmdline->GetValue("f").toString();

        if (!uri.isEmpty())
        {
            if (cmdline.data()->GetValue("probe").isValid())
                ret = TorcUtils::Probe(uri);
            else if (cmdline.data()->GetValue("play").isValid())
                ret = TorcUtils::Play(uri);
        }
    }

    TorcLocalContext::TearDown();

    return ret;
}


